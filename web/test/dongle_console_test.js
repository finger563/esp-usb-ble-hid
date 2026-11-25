#!/usr/bin/env node
// Unit tests for the pure logic of web/dongle_console.html. Dependency-free:
//
//   node web/test/dongle_console_test.js
//
// - the espp.gamepad_map v1 codec block (between `// --- begin gamepad map
//   codec` and `// --- end gamepad map codec ---`, a copy of espp's
//   components/gamepad_map/web/gamepad_map_console.html block): byte vectors
//   derived by hand from espp's gamepad_map_protocol.hpp (layouts),
//   gamepad_state.hpp (control order) and gamepad_map.hpp (rule kinds,
//   GamepadMap::valid(), the builder helpers behind the presets), plus the
//   rumble options (still v1: INFO / MAP flags bit1, SET_MAP's optional
//   trailing block, TEST_RUMBLE behind INFO flags bit2), whose vectors are
//   espp's C++ goldens (gamepad_map_protocol_host_test.cpp) and the
//   RumbleOptions::apply() parity table (gamepad_map_host_test.cpp), as in
//   espp's gamepad_map_codec_test.js. Fields added after v1 appear in a decoded
//   object only when the device sent them, so the v1 expectations stay as
//   they were;
// - the Full update block (between `// --- begin full update plan` and
//   `// --- end full update plan ---`): manifest.json -> flash plan, the local
//   file fallback, partition table / image checks, MD5, the esptool-js read
//   fix and the "offer the Full update" decision;
// - the device-config INFO flag the decision reads (the whole protocol script,
//   loaded the way web/README.md describes), the SET_SETTINGS payload without
//   the v3 stick / button keys, and that every inline script parses.
"use strict";
const fs = require("fs");
const path = require("path");
const assert = require("assert");
const crypto = require("crypto");

const html = fs.readFileSync(path.join(__dirname, "..", "dongle_console.html"), "utf8");
function block(beginMarker, endMarker) {
  const begin = html.indexOf(beginMarker), end = html.indexOf(endMarker);
  assert.ok(begin > 0 && end > begin, "markers " + beginMarker.trim() + " not found");
  assert.strictEqual(html.indexOf(beginMarker, begin + 1), -1, "marker " + beginMarker.trim() + " appears twice");
  const b = html.slice(begin, end);
  assert.ok(!/\b(document|window|els|navigator|location)\b/.test(b.replace(/\/\/.*$/gm, "")), beginMarker.trim() + ": the block must not touch the DOM / page state");
  return b;
}
const G = new Function(block("    // --- begin gamepad map codec", "    // --- end gamepad map codec ---") + `
  return { T_GET_INFO, T_GET_MAP, T_SET_MAP, T_ERASE_PROFILE, T_LIST_PROFILES, T_SET_STREAM, T_RESET_ALL, T_TEST_RUMBLE,
           T_INFO, T_MAP, T_OK, T_ERROR, T_PROFILES, T_INPUT, BUTTON_COUNT, AXIS_COUNT, B, A, K,
           RULE_NONE, KEEP_SOURCE, AXIS_MAX, STATE_SIZE, RULE_SIZE, PRESETS, buttonLabel, axisLabel, percentToParam, paramToPercent,
           GmReader, GmWriter, encodeState, encodeGetMap, encodeSetMap, encodeEraseProfile, encodeSetStream,
           decodeInfo, decodeMapReply, decodeProfiles, decodeInput, decodeOk, decodeError,
           describeTarget, sameTarget, validateTarget, gmLimits, validateRule, validateMap,
           ruleButton, ruleAxisToButton, ruleButtonToAxis, ruleAxis, changeRuleKind, describeRule,
           learnBaseline, learnStep, applyLearnHit,
           FLAG_RUMBLE_OPTIONS, FLAG_RUMBLE, FLAG_TEST_RUMBLE, encodeTestRumble, RUMBLE_SIZE, RUMBLE_OFF, RUMBLE_SWAP_MOTORS, RUMBLE_KNOWN_FLAGS,
           MAX_RUMBLE_STRENGTH, NEUTRAL_RUMBLE, putRumble, readRumble, validateRumble, applyRumble, describeRumble };
`)();
const P = new Function(block("    // --- begin full update plan", "    // --- end full update plan ---") + `
  return { FULL_FLASH_CHIP, ROM_USB_VID, parseOffset, parseFlashManifest, flashFileUrls, buildLocalFlashPlan, parsePartitionTable,
           checkFlashImages, md5Hex, bytesToBinaryString, binaryStringToBytes, describeRomPort, hasGamepadMapModule,
           fullUpdateReason, fixEsptoolReadTimeout };
`)();

// Every inline script must parse, alone and together (classic scripts share
// one global scope: a name declared twice would throw when the page loads).
const scripts = [...html.matchAll(/<script id="(\w+)">([\s\S]*?)<\/script>/g)];
assert.deepStrictEqual(scripts.map((m) => m[1]), ["proto", "app"], "expected the proto + app inline scripts");
for (const [, id, src] of scripts) assert.doesNotThrow(() => new Function(src), id + " does not parse");
new Function(scripts.map((m) => m[2]).join("\n"));
// The protocol script loads under node (module.exports), as web/README.md says.
const mod = { exports: {} };
new Function("module", scripts[0][2])(mod);
const D = mod.exports;

let passed = 0;
const pending = [];
function test(name, fn) {
  const r = fn();
  if (r && typeof r.then === "function") pending.push(r.then(() => { passed++; console.log("PASS " + name); }));
  else { passed++; console.log("PASS " + name); }
}
const bytes = (...xs) => new Uint8Array(xs.flat());
const str = (s) => { const b = new TextEncoder().encode(s); return [b.length, ...b]; };
const eq = (a, b, msg) => assert.deepStrictEqual(Array.from(a), Array.from(b), msg);
// every strict prefix of a valid device -> host payload is rejected; trailing
// bytes after the known fields are ignored (the protocol's compatibility rule)
function rejectsTruncation(decode, p, name) {
  for (let n = 0; n < p.length; n++) assert.strictEqual(decode(p.subarray(0, n)), null, name + ": prefix of " + n + " bytes accepted");
  const whole = decode(p);
  assert.notStrictEqual(whole, null, name + ": the payload itself is rejected");
  assert.deepStrictEqual(decode(bytes([...p, 0xAA, 0xBB])), whole, name + ": trailing bytes changed the result");
}

// ===================== espp.gamepad_map v1 codec =====================
test("gamepad map: constants mirror gamepad_map_protocol.hpp / gamepad_state.hpp", () => {
  assert.deepStrictEqual([G.T_GET_INFO, G.T_GET_MAP, G.T_SET_MAP, G.T_ERASE_PROFILE, G.T_LIST_PROFILES, G.T_SET_STREAM, G.T_RESET_ALL], [1, 2, 3, 4, 5, 6, 7]);
  assert.deepStrictEqual([G.T_INFO, G.T_MAP, G.T_OK, G.T_ERROR, G.T_PROFILES, G.T_INPUT], [0x81, 0x82, 0x83, 0x84, 0x85, 0x86]);
  assert.strictEqual(G.BUTTON_COUNT, 22); assert.strictEqual(G.AXIS_COUNT, 6);
  assert.strictEqual(G.B.SOUTH, 0); assert.strictEqual(G.B.CAPTURE, 13); assert.strictEqual(G.B.UP, 14); assert.strictEqual(G.B.PADDLE4, 21);
  assert.strictEqual(G.A.LEFT_X, 0); assert.strictEqual(G.A.RIGHT_Y, 3); assert.strictEqual(G.A.LEFT_TRIGGER, 4); assert.strictEqual(G.A.RIGHT_TRIGGER, 5);
  assert.deepStrictEqual([G.K.BUTTON_TO_BUTTON, G.K.AXIS_TO_BUTTON, G.K.BUTTON_TO_AXIS, G.K.AXIS_TO_AXIS], [0, 1, 2, 3]);
  assert.deepStrictEqual([G.RULE_NONE, G.KEEP_SOURCE, G.AXIS_MAX, G.STATE_SIZE, G.RULE_SIZE], [0xFF, 0x01, 32767, 16, 6]);
  assert.strictEqual(G.buttonLabel(G.B.EAST), "East (Xbox B / Nintendo A / PS Circle)");
  assert.strictEqual(G.axisLabel(G.A.LEFT_Y), "Left stick Y (- up / + down)");
  // the module the console resolves: espp.gamepad_map v1 at the published id 11
  assert.strictEqual(D.MODULE.GAMEPAD_MAP, 11);
  assert.strictEqual(D.PROTOCOL.GAMEPAD_MAP, "espp.gamepad_map");
  const spec = D.MODULE_SPEC[11];
  assert.ok(spec && spec.protocol === "espp.gamepad_map" && spec.version === 1 && spec.correlates && spec.errorType === 0x84);
  assert.deepStrictEqual(spec.parseError(bytes([3, 22, 0, 0, 0, ...new TextEncoder().encode("bad")])), { request: 3, code: 22, message: "bad" });
});

// dead zones 10 / 20 %, East presses South; Left stick X <= -50 % presses d-pad Left (keep source)
const MAP = { leftDeadzone: 10, rightDeadzone: 20, rules: [
  { kind: 0, source: 1, target: 0, flags: 0, param: 0 },
  { kind: 1, source: 0, target: 16, flags: 1, param: -16384 }] };
const MAP_BYTES = [10, 20, 2, /* rule 0 */ 0, 1, 0, 0, 0x00, 0x00, /* rule 1 */ 1, 0, 16, 1, 0x00, 0xC0];
const XBOX = { profile: true, vid: 0x045e, pid: 0x0b13, name: "Xbox" };
const XBOX_TARGET = [1, 0x5e, 0x04, 0x13, 0x0b, ...str("Xbox")];

test("gamepad map: GET_MAP / SET_MAP / MAP (target + map, 6 bytes per rule)", () => {
  eq(G.encodeGetMap({ profile: false }), [0]);
  eq(G.encodeGetMap(XBOX), XBOX_TARGET);
  eq(G.encodeSetMap({ profile: false }, MAP), [0, ...MAP_BYTES]);
  eq(G.encodeSetMap(XBOX, MAP), [...XBOX_TARGET, ...MAP_BYTES]);
  assert.deepStrictEqual(G.decodeMapReply(bytes([0, 1, ...MAP_BYTES])), { target: { profile: false }, exists: true, map: MAP });
  assert.deepStrictEqual(G.decodeMapReply(bytes([...XBOX_TARGET, 0, 0, 0, 0])), { target: XBOX, exists: false, map: { leftDeadzone: 0, rightDeadzone: 0, rules: [] } });
  rejectsTruncation(G.decodeMapReply, bytes([...XBOX_TARGET, 1, ...MAP_BYTES]), "MAP");
  assert.strictEqual(G.decodeMapReply(bytes([2, 1, ...MAP_BYTES])), null, "target kind 2 is not a target");
  // param is a signed i16: -32767 = 0x8001, 400 = 0x0190
  const ext = { leftDeadzone: 0, rightDeadzone: 0, rules: [{ kind: 2, source: 14, target: 1, flags: 1, param: -32767 }, { kind: 3, source: 2, target: 3, flags: 0, param: 400 }] };
  eq(G.encodeSetMap({ profile: false }, ext), [0, 0, 0, 2, 2, 14, 1, 1, 0x01, 0x80, 3, 2, 3, 0, 0x90, 0x01]);
  // a name-only profile (VID and PID 0), UTF-8
  eq(G.encodeGetMap({ profile: true, vid: 0, pid: 0, name: "Pad é" }), [1, 0, 0, 0, 0, 6, 0x50, 0x61, 0x64, 0x20, 0xC3, 0xA9]);
});

test("gamepad map: ERASE_PROFILE / SET_STREAM payloads", () => {
  eq(G.encodeEraseProfile(XBOX), [0x5e, 0x04, 0x13, 0x0b, ...str("Xbox")]);
  eq(G.encodeSetStream(true, 50), [1, 50, 0]);
  eq(G.encodeSetStream(false, 0x1234), [0, 0x34, 0x12]);
  const long = G.encodeEraseProfile({ vid: 1, pid: 2, name: "x".repeat(300) });
  assert.strictEqual(long.length, 4 + 1 + 255, "names are truncated to 255 bytes like put_str()");
});

test("gamepad map: INFO / PROFILES / INPUT / OK / ERROR", () => {
  const none = bytes([22, 6, 64, 16, 3, 0]);
  assert.deepStrictEqual(G.decodeInfo(none), { buttonCount: 22, axisCount: 6, maxRules: 64, maxProfiles: 16, profileCount: 3, controller: null });
  rejectsTruncation(G.decodeInfo, none, "INFO");
  const withCtl = bytes([22, 6, 64, 16, 3, 1, 0x5e, 0x04, 0x13, 0x0b, ...str("Xbox"), 1]);
  assert.deepStrictEqual(G.decodeInfo(withCtl).controller, { vid: 0x045e, pid: 0x0b13, name: "Xbox", hasProfile: true });
  rejectsTruncation(G.decodeInfo, withCtl, "INFO (controller)");
  const prof = bytes([2, 0x5e, 0x04, 0x13, 0x0b, ...str("Xbox"), 4, 0, 0, 0, 0, ...str("Pad"), 0]);
  assert.deepStrictEqual(G.decodeProfiles(prof), [{ vid: 0x045e, pid: 0x0b13, name: "Xbox", ruleCount: 4 }, { vid: 0, pid: 0, name: "Pad", ruleCount: 0 }]);
  rejectsTruncation(G.decodeProfiles, prof, "PROFILES");
  // INPUT = [flags][raw 16 B][mapped 16 B]; state = [buttons u32][6 x i16]
  const raw = [0x03, 0, 0, 0, 0x01, 0x80, 0xFF, 0x7F, 0, 0, 0x34, 0x12, 0, 0, 0x00, 0x40];
  const mapped = [0, 0, 0x21, 0x80, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0];
  const input = bytes([3, ...raw, ...mapped]);
  assert.deepStrictEqual(G.decodeInput(input), { connected: true, changed: true,
    raw: { buttons: 3, axes: [-32767, 32767, 0, 0x1234, 0, 16384] },
    mapped: { buttons: ((1 << 16) | (1 << 21) | 0x80000000) >>> 0, axes: [0, 0, 0, 0, 0, 0] } });
  rejectsTruncation(G.decodeInput, input, "INPUT");
  eq(G.encodeState(G.decodeInput(input).raw), raw);
  assert.deepStrictEqual(G.decodeOk(bytes([3])), { requestType: 3 });
  assert.deepStrictEqual(G.decodeError(bytes([3, 22, 0, 0, 0, ...new TextEncoder().encode("invalid map")])), { requestType: 3, code: 22, message: "invalid map" });
  assert.strictEqual(G.decodeError(bytes([3, 22, 0, 0])), null);
});

test("gamepad map: local validation mirrors GamepadMap::valid() / GamepadMapRule::valid()", () => {
  const lim = G.gmLimits({ maxRules: 4, buttonCount: 22, axisCount: 6 });
  assert.deepStrictEqual(G.validateMap(MAP, lim), []);
  assert.deepStrictEqual(G.gmLimits(null), { maxRules: 64, buttonCount: 22, axisCount: 6 });
  const r = (o) => ({ kind: 0, source: 0, target: 0, flags: 0, param: 0, ...o });
  const why = (o) => G.validateRule(r(o), lim);
  assert.strictEqual(why({}), null);
  assert.strictEqual(why({ kind: 4 }), "unknown rule kind");
  assert.strictEqual(why({ flags: 2 }), "unknown flags");
  assert.strictEqual(why({ source: 22 }), "source out of range");
  assert.strictEqual(why({ target: 22 }), "target out of range");
  assert.strictEqual(why({ target: 0xFF }), null, "NONE removes the source");
  assert.strictEqual(why({ target: 0xFF, flags: 1 }), "a rule that keeps its source needs a target");
  assert.strictEqual(why({ kind: 1, param: 0 }), "axis -> button needs a non-zero threshold");
  assert.strictEqual(why({ kind: 1, source: 4, param: -100 }), "a trigger never goes below 0: its threshold must be positive");
  assert.strictEqual(why({ kind: 3, param: 401 }), "axis -> axis scale out of range");
  assert.strictEqual(why({ kind: 3, param: -400 }), null);
  const m = { leftDeadzone: 91, rightDeadzone: 0, rules: [r({}), r({ kind: 1 }), r({}), r({}), r({ source: 30 })] };
  assert.deepStrictEqual(G.validateMap(m, lim), [
    { rule: -1, why: "dead zone out of range" },
    { rule: -1, why: "too many rules (5; the device takes at most 4)" },
    { rule: 1, why: "axis -> button needs a non-zero threshold" },
    { rule: 4, why: "source out of range" }]);
  assert.ok(/needs a name/.test(G.validateTarget({ profile: true, vid: 0, pid: 0, name: "" })));
  assert.ok(G.sameTarget(XBOX, { ...XBOX, name: "Xbox Wireless Controller" }), "a VID:PID profile matches by VID:PID");
  assert.strictEqual(G.describeTarget(XBOX), "profile 045e:0b13 “Xbox”");
});

test("gamepad map: presets encode like the GamepadMap builder helpers; learn + rule edits", () => {
  const Pr = Object.fromEntries(G.PRESETS.map((p) => [p.id, p.rules()]));
  const enc = (rules) => Array.from(G.encodeSetMap({ profile: false }, { leftDeadzone: 0, rightDeadzone: 0, rules })).slice(4);
  assert.deepStrictEqual(enc(Pr.swapSouthEast), [0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0]);         // swap(South, East)
  assert.deepStrictEqual(enc(Pr.invertLeftY), [3, 1, 1, 0, 0x9C, 0xFF]);                       // invert(LeftY): -100
  assert.deepStrictEqual(enc(Pr.leftStickToDpad), [1, 0, 16, 1, 0x00, 0xC0, 1, 0, 17, 1, 0x00, 0x40, 1, 1, 14, 1, 0x00, 0xC0, 1, 1, 15, 1, 0x00, 0x40]);
  assert.deepStrictEqual(enc(Pr.triggersToButtons), [1, 4, 6, 1, 0x33, 0x33, 1, 5, 7, 1, 0x33, 0x33]);
  for (const p of G.PRESETS) assert.deepStrictEqual(G.validateMap({ leftDeadzone: 0, rightDeadzone: 0, rules: p.rules() }), [], p.id + " is valid");
  assert.strictEqual(G.percentToParam(50), 16384); assert.strictEqual(G.paramToPercent(13107), 40);
  const st = (buttons, axes = [0, 0, 0, 0, 0, 0]) => ({ buttons, axes });
  const base = G.learnBaseline(st(1 << G.B.EAST));
  assert.deepStrictEqual(G.learnStep(base, st((1 << G.B.EAST) | (1 << G.B.NORTH))).hit, { type: "button", index: G.B.NORTH });
  assert.deepStrictEqual(G.learnStep(G.learnBaseline(st(0)), st(0, [-20000, 0, 0, 0, 0, 0])).hit, { type: "axis", index: G.A.LEFT_X, sign: -1 });
  assert.deepStrictEqual(G.applyLearnHit(G.ruleButton(G.B.SOUTH, G.B.LEFT), { type: "axis", index: G.A.LEFT_X, sign: -1 }),
    { kind: G.K.AXIS_TO_BUTTON, source: G.A.LEFT_X, target: G.B.LEFT, flags: 0, param: -16384 });
  assert.deepStrictEqual(G.changeRuleKind(G.ruleButton(G.B.EAST, G.B.NORTH, true), G.K.AXIS_TO_BUTTON), { kind: 1, source: 0, target: G.B.NORTH, flags: 1, param: 16384 });
  assert.strictEqual(G.describeRule(G.ruleAxis(G.A.LEFT_Y, G.A.LEFT_Y, -100)), "Left stick Y is inverted");
});

// ---- rumble options (still v1: INFO / MAP flags bit1, SET_MAP's optional trailing block, TEST_RUMBLE) ----
const OFF = 0x01, SWAP = 0x02;
const EMPTY_MAP = { leftDeadzone: 0, rightDeadzone: 0, rules: [] };

test("gamepad map: rumble constants mirror rumble_options.hpp / gamepad_map_protocol.hpp", () => {
  assert.deepStrictEqual([G.FLAG_RUMBLE_OPTIONS, G.FLAG_RUMBLE, G.RUMBLE_SIZE], [0x02, 0x02, 2]);
  assert.deepStrictEqual([G.RUMBLE_OFF, G.RUMBLE_SWAP_MOTORS, G.RUMBLE_KNOWN_FLAGS, G.MAX_RUMBLE_STRENGTH], [OFF, SWAP, 0x03, 200]);
  assert.deepStrictEqual(G.NEUTRAL_RUMBLE, { flags: 0, strength: 100 });
  assert.ok(Object.isFrozen(G.NEUTRAL_RUMBLE));
  // [flags u8][strength_percent u8], raw both ways (layout only: reserved bits and 250 % pass)
  const w = new G.GmWriter(); G.putRumble(w, { flags: 0xFF, strength: 250 });
  eq(w.bytes(), [0xFF, 0xFA]);
  assert.deepStrictEqual(G.readRumble(new G.GmReader(bytes([0xFF, 0xFA]))), { flags: 0xFF, strength: 250 });
  assert.strictEqual(G.readRumble(new G.GmReader(bytes([0x01]))), null);
  assert.strictEqual(G.readRumble(new G.GmReader(bytes([]))), null);
  // the console still speaks espp.gamepad_map v1 (rumble options did not bump it)
  assert.strictEqual(D.MODULE_SPEC[11].version, 1);
  assert.strictEqual(D.MODULE_SPEC[11].names[0x08], "TEST_RUMBLE");
});

test("gamepad map: SET_MAP = [target][map], then the optional [rumble] block", () => {
  // the C++ golden (gamepad_map_protocol_host_test.cpp): profile 1949:0402 "BSP",
  // dead zones 10 / 5, south -> east, left_y <= -16384 -> up (keep)
  const BSP = { profile: true, vid: 0x1949, pid: 0x0402, name: "BSP" };
  const BSP_MAP = { leftDeadzone: 10, rightDeadzone: 5, rules: [
    { kind: 0, source: 0, target: 1, flags: 0, param: 0 },
    { kind: 1, source: 1, target: 14, flags: 1, param: -16384 }] };
  const golden = [0x01, 0x49, 0x19, 0x02, 0x04, 0x03, 0x42, 0x53, 0x50, 10, 5, 2,
                  0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x01, 0x01, 0x0e, 0x01, 0x00, 0xc0];
  // no rumble: byte-identical to the v1 form (the dongle keeps the stored options)
  eq(G.encodeSetMap(BSP, BSP_MAP), golden);
  eq(G.encodeSetMap(BSP, BSP_MAP, null), golden);
  eq(G.encodeSetMap(BSP, BSP_MAP, undefined), golden);
  eq(G.encodeSetMap(XBOX, MAP, null), [...XBOX_TARGET, ...MAP_BYTES]);
  // the editor's map object carries `rumble`: only the third argument sends it
  eq(G.encodeSetMap(XBOX, { ...MAP, rumble: { flags: SWAP, strength: 150 } }), [...XBOX_TARGET, ...MAP_BYTES]);
  // with rumble options: the 2-byte block appended (RUMBLE_OFF, 80 %)
  eq(G.encodeSetMap(BSP, BSP_MAP, { flags: OFF, strength: 80 }), [...golden, 0x01, 0x50]);
  // default target, empty map, neutral
  eq(G.encodeSetMap({ profile: false }, EMPTY_MAP, G.NEUTRAL_RUMBLE), [0x00, 0x00, 0x00, 0x00, 0x00, 0x64]);
  // unknown flag bits are echoed as they are
  eq(G.encodeSetMap(XBOX, MAP, { flags: 0x86, strength: 100 }), [...XBOX_TARGET, ...MAP_BYTES, 0x86, 0x64]);
  assert.strictEqual(G.encodeSetMap(XBOX, MAP, G.NEUTRAL_RUMBLE).length, XBOX_TARGET.length + MAP_BYTES.length + G.RUMBLE_SIZE);
  // the page's exported encoder is the codec block's
  eq(D.encodeSetMap(BSP, BSP_MAP, { flags: OFF, strength: 80 }), [...golden, 0x01, 0x50]);
  eq(D.encodeSetMap(BSP, BSP_MAP), golden);
});

test("gamepad map: MAP carries a rumble block when flags bit1 is set", () => {
  // C++ golden: default target, exists, empty map, {SWAP_MOTORS, 150}
  const def = bytes([0x00, 0x03, 0x00, 0x00, 0x00, 0x02, 0x96]);
  assert.deepStrictEqual(G.decodeMapReply(def), { target: { profile: false }, exists: true, map: EMPTY_MAP, rumble: { flags: SWAP, strength: 150 } });
  // every prefix fails, the one ending right after the map included; bytes after the block are ignored
  rejectsTruncation(G.decodeMapReply, def, "MAP (rumble)");
  // C++ golden: a missing profile (exists 0) answers with the default map and the default (here neutral) rumble
  const missing = bytes([...XBOX_TARGET, 0x02, 0x00, 0x00, 0x00, 0x00, 0x64]);
  assert.deepStrictEqual(G.decodeMapReply(missing), { target: XBOX, exists: false, map: EMPTY_MAP, rumble: { flags: 0, strength: 100 } });
  rejectsTruncation(G.decodeMapReply, missing, "MAP (missing profile, rumble)");
  // a stored profile with rules and rumble options
  const full = bytes([...XBOX_TARGET, 0x03, ...MAP_BYTES, 0x01, 0x50]);
  assert.deepStrictEqual(G.decodeMapReply(full), { target: XBOX, exists: true, map: MAP, rumble: { flags: OFF, strength: 80 } });
  rejectsTruncation(G.decodeMapReply, full, "MAP (profile, rumble)");
  // bit1 with no block, or a 1-byte block: truncated
  assert.strictEqual(G.decodeMapReply(bytes([0x00, 0x03, 0x00, 0x00, 0x00])), null);
  assert.strictEqual(G.decodeMapReply(bytes([0x00, 0x02, 0x00, 0x00, 0x00, 0x02])), null);
  // without bit1 there is no rumble (no key at all), even if bytes trail: what
  // v4 firmware without rumble options sends (the editor then keeps the Rumble row disabled)
  const v1 = G.decodeMapReply(bytes([0x00, 0x01, 0x00, 0x00, 0x00, 0xAA, 0xBB]));
  assert.deepStrictEqual(v1, { target: { profile: false }, exists: true, map: EMPTY_MAP });
  assert.ok(!("rumble" in v1), "a v1 MAP decodes without a rumble key");
  assert.strictEqual(G.decodeMapReply(bytes([0, 0xFC, 0, 0, 0])).exists, false, "the reserved MAP flag bits carry nothing");
  // the reserved MAP flag bits next to bit1 change nothing
  assert.deepStrictEqual(G.decodeMapReply(bytes([0x00, 0xFF, 0x00, 0x00, 0x00, 0x02, 0x96])), G.decodeMapReply(def));
  // the block is kept raw (layout only): reserved bits and an out-of-range
  // strength decode, and reserved bits go back to the dongle unchanged
  const odd = G.decodeMapReply(bytes([0x00, 0x03, 0x00, 0x00, 0x00, 0x86, 0xFA]));
  assert.deepStrictEqual(odd.rumble, { flags: 0x86, strength: 250 });
  assert.notStrictEqual(G.validateRumble(odd.rumble), null, "250 % is refused locally");
  eq(G.encodeSetMap(odd.target, odd.map, { ...odd.rumble, strength: 120 }), [0x00, 0x00, 0x00, 0x00, 0x86, 120]);
  assert.deepStrictEqual(D.decodeMapReply(full), G.decodeMapReply(full));
});

test("gamepad map: TEST_RUMBLE = [rumble], offered behind INFO flags bit2", () => {
  assert.strictEqual(G.T_TEST_RUMBLE, 0x08);
  assert.strictEqual(G.FLAG_TEST_RUMBLE, 0x04);
  // C++ golden: encode_test_rumble({SWAP_MOTORS, 150})
  eq(G.encodeTestRumble({ flags: G.RUMBLE_SWAP_MOTORS, strength: 150 }), [0x02, 0x96]);
  eq(D.encodeTestRumble({ flags: G.RUMBLE_SWAP_MOTORS, strength: 150 }), [0x02, 0x96]);
  // unknown flag bits are sent as edited
  eq(G.encodeTestRumble({ flags: 0x86, strength: 100 }), [0x86, 0x64]);
  // C++ golden: rumble options + test rumble, no controller
  const info = G.decodeInfo(bytes([22, 6, 64, 16, 1, 0x06]));
  assert.strictEqual(info.rumbleOptions, true);
  assert.strictEqual(info.testRumble, true);
  // without bit2 there is no key (a v1 / rumble-only INFO decodes as before)
  assert.ok(!("testRumble" in G.decodeInfo(bytes([22, 6, 64, 16, 1, 0x02]))));
  assert.ok(!("testRumble" in G.decodeInfo(bytes([22, 6, 64, 16, 1, 0x00]))));
});

test("gamepad map: INFO's rumble options capability (flags bit1, no bytes added)", () => {
  // C++ goldens: no controller / the 045e:0b13 "Xbox" controller
  const none = bytes([0x16, 0x06, 0x40, 0x10, 0x01, 0x02]);
  assert.deepStrictEqual(G.decodeInfo(none), { buttonCount: 22, axisCount: 6, maxRules: 64, maxProfiles: 16, profileCount: 1, rumbleOptions: true, controller: null });
  rejectsTruncation(G.decodeInfo, none, "INFO (rumble options, no controller)");
  const withCtl = bytes([0x16, 0x06, 0x40, 0x10, 0x01, 0x03, 0x5e, 0x04, 0x13, 0x0b, 0x04, 0x58, 0x62, 0x6f, 0x78, 0x01]);
  assert.deepStrictEqual(G.decodeInfo(withCtl), { buttonCount: 22, axisCount: 6, maxRules: 64, maxProfiles: 16, profileCount: 1, rumbleOptions: true,
    controller: { vid: 0x045e, pid: 0x0b13, name: "Xbox", hasProfile: true } });
  rejectsTruncation(G.decodeInfo, withCtl, "INFO (rumble options, controller)");
  // only bit1 means it: a v1 INFO and a reserved bit decode without the key
  const v1 = G.decodeInfo(bytes([22, 6, 64, 16, 3, 0x00]));
  assert.ok(!("rumbleOptions" in v1), "a v1 INFO decodes without a rumbleOptions key");
  const reserved = G.decodeInfo(bytes([22, 6, 64, 16, 3, 0x80]));
  assert.ok(!("rumbleOptions" in reserved));
  assert.strictEqual(reserved.controller, null);
});

test("gamepad map: rumble validation mirrors RumbleOptions::valid() (the strength; reserved bits pass)", () => {
  const why = "Rumble: strength out of range (0..200 %)";
  for (const s of [0, 1, 100, 137, 199, 200]) assert.strictEqual(G.validateRumble({ flags: 0, strength: s }), null, s + " %");
  for (const s of [201, 255, -1, 1.5, NaN, Infinity, "100", undefined, null]) assert.strictEqual(G.validateRumble({ flags: 0, strength: s }), why, String(s));
  // unknown flag bits are never flagged (echoed as loaded)
  assert.strictEqual(G.validateRumble({ flags: 0x84, strength: 100 }), null);
  assert.strictEqual(G.validateRumble({ flags: 0xFF, strength: 0 }), null);
  assert.strictEqual(G.validateRumble(G.NEUTRAL_RUMBLE), null);
  assert.strictEqual(G.validateRumble(null), null, "no rumble options: nothing to check");
  assert.strictEqual(D.validateRumble({ flags: 0, strength: 201 }), why);
});

test("gamepad map: applyRumble mirrors RumbleOptions::apply() (the parity table + properties)", () => {
  // [{flags, strength}, in {Strong, Weak, LT, RT}, out] -- the table of espp's gamepad_map_host_test.cpp
  const table = [
    [[0, 100], [200, 10, 5, 7], [200, 10, 5, 7]],
    [[OFF, 100], [200, 10, 5, 7], [0, 0, 0, 0]],
    [[0, 0], [200, 10, 5, 7], [0, 0, 0, 0]],
    [[0, 50], [255, 1, 3, 0], [128, 1, 2, 0]],
    [[0, 1], [255, 1, 0, 0], [3, 0, 0, 0]],
    [[0, 49], [1, 0, 0, 0], [0, 0, 0, 0]],
    [[0, 150], [100, 200, 128, 0], [150, 255, 192, 0]],
    [[0, 200], [127, 128, 255, 100], [254, 255, 255, 200]],
    [[SWAP, 100], [200, 10, 5, 7], [10, 200, 5, 7]],
    [[SWAP, 50], [200, 10, 5, 7], [5, 100, 3, 4]],
    [[0xFC, 100], [200, 10, 5, 7], [200, 10, 5, 7]], // unknown bits only: ignored
    [[0, 250], [100, 0, 0, 0], [200, 0, 0, 0]],      // invalid: behaves as 200
  ];
  for (const [[flags, strength], input, out] of table) {
    assert.deepStrictEqual(G.applyRumble({ flags, strength }, input), out, JSON.stringify([flags, strength, input]));
    assert.deepStrictEqual(D.applyRumble({ flags, strength }, input), out);
  }
  const o = (flags, strength) => ({ flags, strength });
  // 100 % without flags is the exact identity, every level on every motor
  for (let v = 0; v < 256; v++)
    for (let m = 0; m < 4; m++) { const lv = [0, 0, 0, 0]; lv[m] = v; assert.deepStrictEqual(G.applyRumble(G.NEUTRAL_RUMBLE, lv), lv); }
  // off and 0 % give all zero; stops stay stops for every option value
  for (let s = 0; s <= 200; s++) {
    assert.deepStrictEqual(G.applyRumble(o(OFF, s), [255, 255, 255, 255]), [0, 0, 0, 0]);
    for (let f = 0; f < 4; f++) assert.deepStrictEqual(G.applyRumble(o(f, s), [0, 0, 0, 0]), [0, 0, 0, 0]);
  }
  assert.deepStrictEqual(G.applyRumble(o(0, 0), [255, 255, 255, 255]), [0, 0, 0, 0]);
  // monotonic in level and in strength (on Strong)
  for (let s = 0; s <= 200; s++)
    for (let v = 1; v < 256; v++) {
      assert.ok(G.applyRumble(o(0, s), [v, 0, 0, 0])[0] >= G.applyRumble(o(0, s), [v - 1, 0, 0, 0])[0]);
      if (s > 0) assert.ok(G.applyRumble(o(0, s), [v, 0, 0, 0])[0] >= G.applyRumble(o(0, s - 1), [v, 0, 0, 0])[0]);
    }
  // swap twice at 100 % is the identity; swap and scaling commute; apply() is not idempotent
  const req = [200, 10, 5, 7];
  assert.deepStrictEqual(G.applyRumble(o(SWAP, 100), G.applyRumble(o(SWAP, 100), req)), req);
  assert.deepStrictEqual(G.applyRumble(o(SWAP, 150), req), G.applyRumble(o(SWAP, 100), G.applyRumble(o(0, 150), req)));
  assert.notDeepStrictEqual(G.applyRumble(o(0, 50), G.applyRumble(o(0, 50), req)), G.applyRumble(o(0, 50), req));
  // the dongle's test pattern (strong only at 128, then weak only at 128) through the options
  assert.deepStrictEqual(G.applyRumble(o(SWAP, 100), [128, 0, 0, 0]), [0, 128, 0, 0], "swap: the strong step plays on the weak motor");
  assert.deepStrictEqual(G.applyRumble(o(0, 200), [0, 128, 0, 0]), [0, 255, 0, 0], "200 %: the weak step saturates");
  assert.deepStrictEqual(G.applyRumble(o(0, 50), [128, 0, 0, 0]), [64, 0, 0, 0]);
});

test("gamepad map: describeRumble", () => {
  assert.strictEqual(G.describeRumble({ flags: 0, strength: 100 }), "100 %");
  assert.strictEqual(G.describeRumble({ flags: SWAP, strength: 150 }), "150 %, strong / weak swapped");
  assert.strictEqual(G.describeRumble({ flags: OFF, strength: 150 }), "off");
  assert.strictEqual(G.describeRumble({ flags: OFF | SWAP, strength: 50 }), "off");
  assert.strictEqual(G.describeRumble({ flags: 0x84, strength: 0 }), "0 %");
  assert.strictEqual(D.describeRumble({ flags: SWAP, strength: 150 }), "150 %, strong / weak swapped");
});

test("Button mapping tab: the Rumble row's controls exist once and start disabled", () => {
  const count = (re) => (html.match(re) || []).length;
  for (const id of ["gmRumbleRow", "gmRumbleOn", "gmRumbleStrength", "gmRumbleStrengthOut", "gmRumbleTicks", "gmRumbleNormal",
                    "gmRumbleSwap", "gmRumbleTest", "gmRumbleNote", "gmDevRumble"])
    assert.strictEqual(count(new RegExp('id="' + id + '"', "g")), 1, id);
  // nothing is editable before a map with rumble options is loaded from firmware that stores them
  for (const id of ["gmRumbleOn", "gmRumbleStrength", "gmRumbleNormal", "gmRumbleSwap", "gmRumbleTest"])
    assert.match(html, new RegExp('id="' + id + '"[^>]*\\bdisabled\\b'), id + " starts disabled");
  assert.match(html, /id="gmRumbleStrength"[^>]*min="0" max="200"[^>]*list="gmRumbleTicks"/);
});

// ===================== Full update: manifest -> flash plan =====================
const MANIFEST = { version: "v4.0.0", chip: "esp32s3", files: [
  { offset: 0x20000, path: "esp-usb-ble-hid.bin" }, { offset: 0, path: "bootloader.bin" },
  { offset: 0x8000, path: "partition-table.bin" }, { offset: 0xf000, path: "ota_data_initial.bin" }] };

test("full update: manifest.json -> plan (sorted, offsets as numbers or hex strings)", () => {
  const plan = P.parseFlashManifest(JSON.stringify(MANIFEST));
  assert.deepStrictEqual(plan, { version: "v4.0.0", chip: "esp32s3", warnings: [], files: [
    { offset: 0, path: "bootloader.bin" }, { offset: 0x8000, path: "partition-table.bin" },
    { offset: 0xf000, path: "ota_data_initial.bin" }, { offset: 0x20000, path: "esp-usb-ble-hid.bin" }] });
  // the parsed object works too; "ESP32-S3" names the same chip; hex string offsets are accepted
  const alt = P.parseFlashManifest({ version: " v4.1.0 ", chip: "ESP32-S3", files: [{ offset: "0x8000", path: "pt.bin" }, { offset: "0", path: "bl.bin" }] });
  assert.deepStrictEqual(alt.files.map((f) => f.offset), [0, 0x8000]);
  assert.strictEqual(alt.version, "v4.1.0");
  // the URLs: next to manifest.json on the same origin
  assert.deepStrictEqual(P.flashFileUrls(plan, "https://finger563.github.io/esp-usb-ble-hid/firmware/latest/manifest.json").map((f) => f.url), [
    "https://finger563.github.io/esp-usb-ble-hid/firmware/latest/bootloader.bin",
    "https://finger563.github.io/esp-usb-ble-hid/firmware/latest/partition-table.bin",
    "https://finger563.github.io/esp-usb-ble-hid/firmware/latest/ota_data_initial.bin",
    "https://finger563.github.io/esp-usb-ble-hid/firmware/latest/esp-usb-ble-hid.bin"]);
  // an image set without a bootloader / partition table is flashable, but flagged
  assert.deepStrictEqual(P.parseFlashManifest({ version: "v4", chip: "esp32s3", files: [{ offset: 0x20000, path: "app.bin" }] }).warnings,
    ["no bootloader image at 0x0", "no partition table at 0x8000 (that is what a full update is for)"]);
});

test("full update: bad / incomplete manifests are refused with a reason", () => {
  const bad = (m, re) => assert.throws(() => P.parseFlashManifest(m), re);
  bad("{not json", /not valid JSON/);
  bad("[]", /not a JSON object/);
  bad("null", /not a JSON object/);
  bad({ chip: "esp32s3", files: MANIFEST.files }, /no version/);
  bad({ version: "v4", files: MANIFEST.files }, /no chip/);
  bad({ version: "v4", chip: "esp32", files: MANIFEST.files }, /for esp32, but the dongle is an ESP32-S3/);
  bad({ version: "v4", chip: "esp32s3" }, /lists no files/);                        // missing files
  bad({ version: "v4", chip: "esp32s3", files: [] }, /lists no files/);
  bad({ version: "v4", chip: "esp32s3", files: [{ offset: 0 }] }, /file #1 has no plain file name/);
  bad({ version: "v4", chip: "esp32s3", files: [{ path: "a.bin" }] }, /file #1 \(a\.bin\) has no valid flash offset/);
  bad({ version: "v4", chip: "esp32s3", files: [{ offset: -4096, path: "a.bin" }] }, /no valid flash offset/);
  bad({ version: "v4", chip: "esp32s3", files: [{ offset: 1.5, path: "a.bin" }] }, /no valid flash offset/);
  bad({ version: "v4", chip: "esp32s3", files: [{ offset: "0x8000oops", path: "a.bin" }] }, /no valid flash offset/);
  bad({ version: "v4", chip: "esp32s3", files: [{ offset: 0x8100, path: "a.bin" }] }, /0x8100\) does not start on a 4 KiB flash sector/);
  bad({ version: "v4", chip: "esp32s3", files: [{ offset: 0, path: "a.bin" }, { offset: "0x0", path: "b.bin" }] }, /offset 0x0 twice/);
  for (const p of ["../app.bin", "dir/app.bin", "https://evil.example/app.bin", ".hidden", "a..b.bin", ""])
    bad({ version: "v4", chip: "esp32s3", files: [{ offset: 0, path: p }] }, /no plain file name/);
});

test("full update: local files -> plan (one input per offset, prefilled 0x0 / 0x8000 / 0xf000 / 0x20000)", () => {
  const d = (n) => new Uint8Array(n);
  const plan = P.buildLocalFlashPlan([
    { offset: "0x0", name: "bootloader.bin", data: d(4) }, { offset: "0x8000", name: "partition-table.bin", data: d(4) },
    { offset: "0xf000", name: "ota_data_initial.bin", data: null }, { offset: " 20000 ", name: "app.bin", data: d(4) }]);
  assert.deepStrictEqual(plan.files.map((f) => [f.offset, f.name]), [[0, "bootloader.bin"], [0x8000, "partition-table.bin"], [0x20000, "app.bin"]]);
  assert.deepStrictEqual(plan.warnings, []);
  assert.throws(() => P.buildLocalFlashPlan([{ offset: "0x0", name: "x", data: null }]), /pick at least one image file/);
  assert.throws(() => P.buildLocalFlashPlan([{ offset: "0x8000oops", name: "pt.bin", data: d(1) }]), /invalid offset "0x8000oops" for pt\.bin/);
  assert.throws(() => P.buildLocalFlashPlan([{ offset: "0x10", name: "pt.bin", data: d(1) }]), /4 KiB flash sector/);
  assert.throws(() => P.buildLocalFlashPlan([{ offset: "0x0", name: "a", data: d(1) }, { offset: "0", name: "b", data: d(1) }]), /two files are set to flash at 0x0/);
});

// gen_esp32part.py layout: [AA 50][type][subtype][offset u32][size u32][label 16][flags u32], then the MD5 entry
function partitionTable(entries, { md5 = true } = {}) {
  const out = [];
  const le = (v) => [v & 255, (v >>> 8) & 255, (v >>> 16) & 255, (v >>> 24) & 255];
  for (const [label, type, subtype, offset, size] of entries) {
    const name = [...Buffer.from(label)]; while (name.length < 16) name.push(0);
    out.push(0xAA, 0x50, type, subtype, ...le(offset), ...le(size), ...name, ...le(0));
  }
  if (md5) { const h = crypto.createHash("md5").update(Buffer.from(out)).digest(); out.push(0xEB, 0xEB, ...new Array(14).fill(0xFF), ...h); }
  while (out.length < 3072) out.push(0xFF);
  return new Uint8Array(out);
}
// partitions.csv (firmware v4)
const V4_TABLE = partitionTable([["nvs", 1, 2, 0x9000, 0x6000], ["otadata", 1, 0, 0xf000, 0x2000], ["phy_init", 1, 1, 0x11000, 0x1000],
  ["ota_0", 0, 0x10, 0x20000, 0x1D0000], ["ota_1", 0, 0x11, 0x1F0000, 0x1D0000], ["coredump", 1, 3, 0x3C0000, 0x20000], ["littlefs", 1, 0x83, 0x3E0000, 0x20000]]);

test("full update: partition table parsing (the image written at 0x8000)", () => {
  // the first entry exactly as gen_esp32part.py writes it for partitions.csv
  eq(V4_TABLE.subarray(0, 16), [0xAA, 0x50, 0x01, 0x02, 0x00, 0x90, 0x00, 0x00, 0x00, 0x60, 0x00, 0x00, 0x6E, 0x76, 0x73, 0x00]);
  const parts = P.parsePartitionTable(V4_TABLE);
  assert.deepStrictEqual(parts.map((p) => [p.label, p.type, p.subtype, p.offset, p.size]), [
    ["nvs", 1, 2, 0x9000, 0x6000], ["otadata", 1, 0, 0xf000, 0x2000], ["phy_init", 1, 1, 0x11000, 0x1000], ["ota_0", 0, 0x10, 0x20000, 0x1D0000],
    ["ota_1", 0, 0x11, 0x1F0000, 0x1D0000], ["coredump", 1, 3, 0x3C0000, 0x20000], ["littlefs", 1, 0x83, 0x3E0000, 0x20000]]);
  assert.strictEqual(P.parsePartitionTable(partitionTable([["nvs", 1, 2, 0x9000, 0x6000]], { md5: false })).length, 1, "erased flash ends the table too");
  assert.throws(() => P.parsePartitionTable(new Uint8Array(64).fill(0x12)), /bad magic/);
  assert.throws(() => P.parsePartitionTable(new Uint8Array(64).fill(0xFF)), /holds no partitions/);
});

test("full update: image checks keep NVS and catch wrong / overlapping images", () => {
  const img = (n, first = 0xE9) => { const b = new Uint8Array(n).fill(0x5A); b[0] = first; return b; };
  const good = () => [{ offset: 0, name: "bootloader.bin", data: img(21200) }, { offset: 0x8000, name: "partition-table.bin", data: V4_TABLE },
                      { offset: 0xf000, name: "ota_data_initial.bin", data: new Uint8Array(0x2000).fill(0xFF) }, { offset: 0x20000, name: "esp-usb-ble-hid.bin", data: img(1454000) }];
  const ok = P.checkFlashImages(good());
  assert.deepStrictEqual([ok.errors, ok.warnings], [[], []]);
  assert.deepStrictEqual(ok.nvs.map((p) => [p.label, p.offset, p.size]), [["nvs", 0x9000, 0x6000]], "NVS (bonds + settings) is reported as kept");
  const errs = (mutate) => { const f = good(); mutate(f); return P.checkFlashImages(f).errors.join(" | "); };
  assert.match(errs((f) => { f[3].data = new Uint8Array(0); }), /esp-usb-ble-hid\.bin is empty/);
  assert.match(errs((f) => { f[0].data = img(0x9000); }), /bootloader\.bin \(0x0\) and partition-table\.bin \(0x8000\) overlap/);
  assert.match(errs((f) => { f.push({ offset: 0xA000, name: "junk.bin", data: img(16) }); }), /junk\.bin \(0xa000\) would overwrite the nvs partition/);
  assert.match(errs((f) => { f[0].data = img(100, 0x00); }), /not a bootloader image/);
  assert.match(errs((f) => { f[3].data = img(1000, 0xFF); }), /not an app image/);
  assert.match(errs((f) => { f[3].data = img(0x1D0001); }), /larger than the ota_0 partition/);
  assert.match(errs((f) => { f[1].data = new Uint8Array(64).fill(0x12); }), /not a partition table: entry 1 has a bad magic/);
  const outside = good(); outside.push({ offset: 0x400000, name: "far.bin", data: img(16) });
  assert.match(P.checkFlashImages(outside).warnings.join(), /far\.bin \(0x400000\) is outside every partition/);
});

test("full update: MD5 (esptool-js verification) and binary strings", () => {
  const enc = (s) => new TextEncoder().encode(s);
  assert.strictEqual(P.md5Hex(enc("")), "d41d8cd98f00b204e9800998ecf8427e");
  assert.strictEqual(P.md5Hex(enc("abc")), "900150983cd24fb0d6963f7d28e17f72");
  assert.strictEqual(P.md5Hex(enc("The quick brown fox jumps over the lazy dog")), "9e107d9d372bb6826bd81d3542a419d6");
  for (const n of [55, 56, 63, 64, 65, 1000, 70001]) {   // around the padding boundaries, and a multi-chunk image
    const b = new Uint8Array(n); for (let i = 0; i < n; i++) b[i] = (i * 131 + 7) & 255;
    assert.strictEqual(P.md5Hex(b), crypto.createHash("md5").update(b).digest("hex"), "md5 of " + n + " bytes");
    const s = P.bytesToBinaryString(b);
    assert.strictEqual(s.length, n);
    eq(P.binaryStringToBytes(s), b);
  }
  assert.strictEqual(P.describeRomPort({ usbVendorId: 0x303a, usbProductId: 0x1001 }), "USB-Serial-JTAG (303a:1001)");
  assert.strictEqual(P.describeRomPort({ usbVendorId: 0x303a, usbProductId: 0x0009 }), "USB-OTG CDC (ROM) (303a:0009)");
  assert.strictEqual(P.describeRomPort({}), "not an Espressif ROM port (?:?)");
  assert.strictEqual(P.ROM_USB_VID, 0x303a);
});

test("full update: the esptool-js read fix keeps a read that timed out for the next call", async () => {
  // a Transport-like class with the 0.5.7 generator semantics: readLoop(timeout) yields chunks
  class FakeTransport { constructor(reader) { this.reader = reader; this.buffer = new Uint8Array(0); } }
  FakeTransport.prototype.readLoop = async function* () { throw new Error("unpatched"); };
  P.fixEsptoolReadTimeout(FakeTransport);
  P.fixEsptoolReadTimeout(FakeTransport); // idempotent
  assert.ok(FakeTransport.prototype.dongleReadFix);
  let deliver = null, reads = 0;
  const reader = { read() { reads++; return new Promise((r) => { deliver = r; }); } };
  const t = new FakeTransport(reader);
  // 1st call: nothing arrives in time -> no data (as before)
  const first = await t.readLoop(20).next();
  assert.strictEqual(first.done, true);
  // the chunk arrives after the timeout: the next call gets it (0.5.7 lost it)
  setTimeout(() => deliver({ value: new Uint8Array([0xC0, 1, 2, 0xC0]), done: false }), 5);
  const second = await t.readLoop(200).next();
  eq(second.value, [0xC0, 1, 2, 0xC0]);
  assert.strictEqual(reads, 1, "the timed-out read was reused, not re-issued");
  // a closed stream ends the loop
  setTimeout(() => deliver({ value: undefined, done: true }), 1);
  assert.strictEqual((await t.readLoop(200).next()).done, true);
});

// ===================== Full update: when to offer it =====================
const disc = (version, modules) => ({ version, device: "ESP USB BLE HID", fw: "", modules: modules.map(([id, protocol]) => ({ id, name: "", app: "", desc: "", protocol: protocol || "", protocolVersion: protocol ? 1 : 0 })) });
const V3_MODULES = [[0x00, "espp.ota"], [0x04, "espp.coredump"], [0x07, "espp.system"], [0x08, "espp.monitor"], [0x10, "esp-usb-ble-hid.device-config"]];
const V4_MODULES = [...V3_MODULES, [0x0B, "espp.gamepad_map"]];
const INFO_V4 = (flags) => {
  const p = [1, flags, 80, 1, 0, 0, 0, 0, ...str("esp-usb-ble-hid"), ...str("v4.0.0"), ...str("T-Dongle-S3"), ...str("v5.5"), ...str("")];
  return D.parseInfo(bytes(p));
};

test("INFO: the optional rumble byte after usb_controller (absent = not reported)", () => {
  const head = [1, 0x02, 80, 1, 0, 0, 0, 0, ...str("esp-usb-ble-hid"), ...str("v4.1.0"), ...str("T-Dongle-S3"), ...str("v6.1"), ...str("SN1")];
  const trailer = [0, 0, 0, 0, 0xFF, 0xFF, 0xFF, 0xFF, 0, 0, 0, 0, 6, ...str(""), 0]; // counters, link state/detail, usb_controller
  const without = D.parseInfo(bytes([...head, ...trailer]));
  assert.strictEqual(without.usbController, 0);
  assert.strictEqual(without.rumbleSupported, null, "older firmware: rumble not reported");
  assert.strictEqual(D.parseInfo(bytes([...head, ...trailer, 1])).rumbleSupported, true);
  assert.strictEqual(D.parseInfo(bytes([...head, ...trailer, 0])).rumbleSupported, false);
  assert.strictEqual(INFO_V4(0x01).rumbleSupported, null, "a payload without any trailer");
});

test("offer the Full update: v3 (no espp.gamepad_map), v4 with INFO flag 0x20, not for v4 without it", () => {
  assert.strictEqual(D.INFO_FLAG.NEEDS_FULL_FLASH, 0x20);
  assert.strictEqual(INFO_V4(0x01 | 0x20).needsFullFlash, true, "INFO flags bit5 = needs_full_flash");
  assert.strictEqual(INFO_V4(0x1F).needsFullFlash, false);
  assert.strictEqual(INFO_V4(0x01 | 0x20).usbMounted, true, "the other flags still decode");
  // v3: discovery without module 11 -> offer
  const v3 = P.fullUpdateReason(disc(2, V3_MODULES), INFO_V4(0x01));
  assert.strictEqual(v3 && v3.code, "no_gamepad_map");
  assert.match(v3.text, /older than v4/);
  // v4 on an old partition table -> offer
  const flagged = P.fullUpdateReason(disc(2, V4_MODULES), INFO_V4(0x01 | 0x20));
  assert.strictEqual(flagged && flagged.code, "needs_full_flash");
  // v4 on the v4 layout -> no
  assert.strictEqual(P.fullUpdateReason(disc(2, V4_MODULES), INFO_V4(0x01)), null);
  assert.strictEqual(P.fullUpdateReason(disc(2, V4_MODULES), null), null);
  // the module found by protocol at any id, or by the published id in a v1 / legacy record
  assert.strictEqual(P.fullUpdateReason(disc(2, [...V3_MODULES, [0x20, "espp.gamepad_map"]]), null), null);
  assert.strictEqual(P.fullUpdateReason(disc(1, [[0x00], [0x07], [0x0B], [0x10]]), null), null);
  assert.strictEqual(P.fullUpdateReason(disc(1, [[0x00], [0x07], [0x10]]), null).code, "no_gamepad_map");
  // no discovery reply: cannot tell -> no prompt (unless INFO says so)
  assert.strictEqual(P.fullUpdateReason(null, null), null);
  assert.strictEqual(P.fullUpdateReason(null, INFO_V4(0x21)).code, "needs_full_flash");
  assert.deepStrictEqual([P.hasGamepadMapModule(null), P.hasGamepadMapModule(disc(2, V3_MODULES)), P.hasGamepadMapModule(disc(2, V4_MODULES))], [null, false, true]);
  // the console resolves the module the same way (published id 11)
  assert.strictEqual(D.resolveModuleId(D.parseDiscovery(discoveryPayload(V4_MODULES)), "espp.gamepad_map", 11), 11);
  assert.strictEqual(D.resolveModuleId(D.parseDiscovery(discoveryPayload([...V3_MODULES, [0x30, "espp.gamepad_map"]])), "espp.gamepad_map", 11), 0x30);
  assert.strictEqual(D.resolveModuleId(D.parseDiscovery(discoveryPayload(V3_MODULES)), "espp.gamepad_map", 11), 11, "absent: the default id (moduleAvailable() then says no)");
});
function discoveryPayload(modules) {
  const out = [2, 0, ...str("ESP USB BLE HID"), ...str("v4.0.0"), modules.length];
  for (const [id, protocol] of modules) out.push(id, ...str("m"), ...str(""), ...str(""), ...str(protocol || ""), 1, 0);
  return bytes(out);
}

test("Settings: a SET_SETTINGS without the v3 stick / button keys is a valid partial TLV list", () => {
  assert.deepStrictEqual(D.LEGACY_MAP_KEYS, ["invert_left_y", "invert_right_y", "swap_ab", "swap_xy", "deadzone_percent"]);
  // device_config_protocol.hpp: [version u8][count u8] then count x [key u8][len u8][value]; partial lists are legal
  eq(D.encodeSettings({ led_brightness: 50 }), [1, 1, 6, 1, 50]);
  eq(D.encodeSettings({ ble_name: "Pad", usb_controller: 1 }), [1, 2, 7, 3, 0x50, 0x61, 0x64, 10, 1, 1]);
  const back = D.decodeSettings(D.encodeSettings({ led_brightness: 50, led_connected_brightness: 25 }));
  assert.deepStrictEqual(back.values, { led_brightness: 50, led_connected_brightness: 25 });
  for (const k of D.LEGACY_MAP_KEYS) assert.ok(!(k in back.values));
});

Promise.all(pending).then(() => console.log("ALL PASS (" + passed + " groups)"), (e) => { console.error(e); process.exit(1); });

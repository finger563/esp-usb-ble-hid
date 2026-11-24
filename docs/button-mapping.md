# Button mapping

You can remap buttons, sticks and triggers inside the dongle from the [web console](web-console.md), with one layout per controller model. The dongle applies your layout before anything reaches the Switch or PC, so it works in every game and on every host, with nothing to install there.

Looking for a particular layout? Go to the [recipes](#recipes).

<!-- screenshot: the Button mapping tab with Live input on, the Raw and Mapped panels side by side, and the two rules of the Swap South / East preset in the Map editor -->

## Before you start

- **You need firmware v4 or newer, with the v4 flash layout.** Older firmware has no button mapping. On older firmware, the tab offers the one-time [Full update](updating.md#full-update) instead. A dongle that got v4 as a normal update but hasn't had the Full update shows *Maps are not saved yet*: it has nowhere to store maps, so **Save to dongle** fails and your edits don't take effect. Until you do the Full update, the dongle keeps using your v3 input settings ([Coming from v3](#coming-from-v3)).
- **Open the web console and connect it** ([web console](web-console.md)).
- **Connect your controller to the dongle.** Live input, Learn and the controller's own profile all need it. You can edit the default map, or a profile for a controller that isn't there, without one.

## Make your first remap

1. Turn the controller on and let it connect, then open the **Button mapping** tab.
2. The **Map to edit** card shows the connected controller and the map it uses. The **Map editor** opens that map: the controller's profile, or, if it has none yet, a *new profile* that starts as a copy of the default map.
3. Press a few buttons and watch **Live input**. The **Raw** panel shows what the controller sends, and the **Mapped** panel shows what the dongle sends on.
4. Click the **Swap South / East (Nintendo A/B)** preset. Two rules appear in the editor. The Mapped panel doesn't change yet.
5. Click **Save to dongle**. The profile is stored and works straight away: press the bottom face button, and the Mapped panel shows **East**.

To undo it, delete the two rules (✕) and save again, or click **Erase this profile** to return the controller to the default map.

## How maps work

- **The default map** applies to every controller that has no profile. It starts empty, so every control passes straight through. The exception is a dongle that had custom input settings on v3 ([Coming from v3](#coming-from-v3)).
- **A profile** belongs to one controller model. The dongle identifies a model by the vendor and product ID (VID:PID) it reports over Bluetooth. A controller that reports neither (0000:0000) is matched by its name instead. Two controllers of the same model share one profile.
- **The dongle picks the map when a controller connects.** Each of your controllers can have its own layout, and you never switch by hand. You can't switch maps from the controller itself, or per game.
- **A new profile starts as a copy of the default map.** After that the two are separate, so later changes to the default map don't reach controllers that have their own profile.
- **Saving takes effect at once.** If you save the map the connected controller uses, the new layout is live as soon as the dongle accepts it. There's no restart.
- **Maps are stored on the dongle.** They go with it to any Switch, PC or Steam Deck, and they're kept through restarts and firmware updates. Forgetting a paired controller doesn't delete its profile.
- **Maps don't change with the USB controller type.** A profile that swaps A and B on the Switch also swaps them in [Xbox 360 mode](hosts.md#switch-to-xbox-360-mode).

A map has a dead zone for each stick and a list of rules. The dongle applies the dead zones first, then all the rules at once:

- **Controls that no rule uses pass through unchanged.**
- **Rules don't chain.** Every rule reads the controller's input, never another rule's result. Give each control all the rules it needs directly.
- **Several rules can feed one target.** Buttons combine, so any of them presses the target. Stick and trigger amounts add up, and the total stops at full.
- **The order of the rules doesn't matter.** The ↑ ↓ buttons only tidy the list.

## Position names

The dongle names buttons by where they sit, not by the letter printed on them, because the letters differ between controller families. The console uses these names everywhere.

| Position | Xbox label | Nintendo label | Switch Pro mode sends | Xbox 360 mode sends |
|---|---|---|---|---|
| South | A | B | A | A |
| East | B | A | B | B |
| West | X | Y | X | X |
| North | Y | X | Y | Y |
| L1 | LB | L | L | LB |
| R1 | RB | R | R | RB |
| L2 | LT, as a button | ZL, as a button | ZL | LT, fully pulled ([Triggers](#triggers)) |
| R2 | RT, as a button | ZR, as a button | ZR | RT, fully pulled ([Triggers](#triggers)) |
| L3 | left stick click | left stick click | left stick click | left stick click |
| R3 | right stick click | right stick click | right stick click | right stick click |
| Select | View | − (Minus) | − | Back |
| Start | Menu | + (Plus) | + | Start |
| Home | Xbox button | Home | Home | Guide |
| Capture | Share | Capture | Capture | nothing (an Xbox 360 controller has no such button) |
| D-pad Up / Down / Left / Right | d-pad | d-pad | d-pad | d-pad |
| Paddle 1 to 4 | back paddles | SL / SR (Paddle 1 and 2) | nothing | nothing |

| Axis | Range in the console | Switch Pro mode sends | Xbox 360 mode sends |
|---|---|---|---|
| Left stick X, Left stick Y | −100 % (left, up) to +100 % (right, down) | left stick | left stick |
| Right stick X, Right stick Y | −100 % (left, up) to +100 % (right, down) | right stick | right stick |
| Left trigger | 0 to 100 % | ZL, pressed once the trigger passes 50 % | LT, analog |
| Right trigger | 0 to 100 % | ZR, pressed once the trigger passes 50 % | RT, analog |

How to read the tables:

- **The labels match by default.** An Xbox controller's A is the Switch's A, even though a Pro Controller has A on the right. For Pro Controller positions, use the [Nintendo positions recipe](#nintendo-button-positions-on-the-switch).
- **PlayStation-style labels follow the same positions.** Cross is South, Circle is East, Square is West and Triangle is North.
- **Up reads negative.** In the console, stick Y grows downward, so pushing a stick up shows a negative value. The dongle turns it the right way up for the host.
- **The Paddle positions do nothing on this dongle.** The dongle never reads a controller's paddles, and neither USB controller has them, so rules that use a Paddle have no effect.

## Rules

### The four kinds of rule

| Kind | In plain words | Parameter | Example, as the editor's *Does* column puts it |
|---|---|---|---|
| button → button | A button presses another button. | none | `East presses South` |
| axis → button | Pushing a stick, or pulling a trigger, past a point presses a button. | A direction and a threshold, 1 to 100 % | `Left stick X below -50% presses D-pad Left` |
| button → axis | Holding a button pushes a stick, or pulls a trigger, by a set amount. | An amount, −100 to +100 % of full travel | `R1 adds +100% to Right trigger` |
| axis → axis | A stick or trigger drives another one, or itself, scaled. A negative scale inverts it. | A scale, −400 to +400 % | `Left stick Y is inverted` |

Click **+ Add rule** to add a row, then set its **Kind**, **Source** (or use [Learn](#learn)), **Target**, **Keep** and **Parameter**. The *Does* column describes the finished rule in words, and ✕ deletes it.

What the parameter means for each kind:

- **axis → button:** pick a direction and a threshold. A stick's X offers *left past (-)* or *right past (+)*, its Y offers *up past (-)* or *down past (+)*, and a trigger only offers *pressed past*. The threshold is a share of full travel.
- **button → axis:** −100 % pushes a stick fully left or up, and +100 % pushes it fully right or down, or pulls a trigger fully. Smaller amounts push only part of the way.
- **axis → axis:** 100 % passes the movement on as it is. 200 % doubles it, so the stick reaches full at half travel. −100 % inverts it. A trigger inverted onto a trigger rests fully pressed and lets go as you pull.

### Move or keep

By default, a rule **moves** its source: the source stops doing its own job. Tick **Keep** to copy the source instead. Take a rule from South to East:

- **With Keep off,** South presses East and no longer presses South. East still presses East as well, because it isn't the source of any rule. Two buttons now press East, and nothing presses South. To exchange two buttons you need a rule in each direction, which is what the Swap presets add.
- **With Keep on,** South presses both South and East.

### Remove a control

Set the target to **none (remove the source)**. A removed button is never pressed, a removed stick axis reads 0, and a removed trigger reads released. You can't tick Keep on a rule whose target is none.

## Dead zones

Each stick has a round dead zone, from 0 to 90 % of full travel. Inside it the stick reads centered. Outside it, the remaining travel is stretched, so a full push still reads full. Use a dead zone for a stick that drifts, or to ignore small movements you don't intend.

While you drag a slider, the dashed circle on the Raw panel's stick shows the dead zone. The Mapped panel changes once you save. Raise the dead zone a little at a time until the Mapped stick stays still when you let go.

Dead zones apply to the physical stick before any rule runs, so a rule that moves the stick elsewhere carries the dead zone with it. Triggers have no dead zone.

## Presets

A preset adds ready-made rules to the map in the editor. It doesn't save on its own, and applying it a second time doesn't undo it. It adds the same rules again. For **Invert** or **Swap sticks**, that makes the stick move twice as far for the same push. To undo a preset, delete its rules.

| Preset | Rules it adds | Use it for |
|---|---|---|
| Swap South / East (Nintendo A/B) | South → East, East → South | Nintendo positions for A and B |
| Swap West / North | West → North, North → West | Nintendo positions for X and Y |
| Invert left Y | Left stick Y → itself at −100 % | Up and down reversed on the left stick |
| Invert right Y | Right stick Y → itself at −100 % | An inverted camera |
| Swap sticks | Each axis of each stick → the same axis of the other stick | Left-handed play |
| Left stick → D-pad | The left stick past 50 % in each direction presses that d-pad direction (Keep on) | Menus and games that only read the d-pad |
| D-pad → left stick | Each d-pad direction pushes the left stick fully that way (Keep on) | Moving with the d-pad in games that only read the stick |
| Triggers → L2 / R2 | Each trigger past 40 % presses L2 / R2 (Keep on) | Triggers that fire sooner: the host sees a full pull from 40 % of the way ([Triggers](#triggers)) |
| L2 / R2 → triggers | L2 / R2 pull their trigger fully (Keep on) | Showing on/off triggers on the trigger bars. The host already gets them from the buttons ([Triggers](#triggers)) |

**Clear rules** removes every rule from the editor. Until you save, **Revert** brings the stored rules back.

Presets that share a source don't combine the way you might expect, because rules read the controller, not each other. For example, Invert left Y together with Swap sticks sends the physical left stick's up and down to both outputs. To invert a swapped stick, change the scale of its swap rule to −100 % instead of adding an Invert preset.

## Learn

You don't have to find a control in the list. Click **Learn** on a rule, then press the button, or push the stick or trigger past 50 %, on the controller. The console fills in the rule's source.

If the control you use is a different type, the rule changes kind to match. Push a stick on a button → button rule, for example, and it becomes an axis → button rule in the direction you pushed.

Learn reads the controller's own input (the Raw panel), and turns Live input on if it was off. It gives up after 15 seconds if nothing is pressed, and **Cancel** stops it sooner.

## Test a controller with Live input

The **Live input** card is also an input tester:

- **Raw (from the controller)** shows what the dongle read from the controller.
- **Mapped (sent to the USB host)** shows the result of the saved map.
- **Buttons** light up by position.
- **Sticks** show −100 to 100 %, with Y growing downward.
- **Triggers** show 0 to 100 %.
- **The update rate** comes from the *every* drop-down: 20, 50, 100 or 250 ms. The stream only runs while the tab is open and visible.

To check a controller in half a minute, pair it and open the tab. Press every button, roll each stick around and pull each trigger, and watch the Raw panel:

- **Every control lights the position you expect.** It works.
- **A control lights the wrong position.** Fix it with a rule in that controller's profile.
- **L2 / R2 light, but the trigger bars stay at 0.** The triggers are on/off buttons. That's fine: the host gets ZL / ZR, or a fully pulled LT / RT, from them ([Triggers](#triggers)).
- **A control lights nothing.** The dongle can't read it yet. Please [report it](controllers.md#report-a-controller).

The Mapped panel shows everything the map produces, which is slightly more than the host receives. The Paddles never reach the host, and Capture doesn't reach it in Xbox 360 mode ([Position names](#position-names)).

<!-- screenshot: Live input during a Learn, with the Raw panel outlined and the "press a button, or push a stick / trigger past 50 %" banner showing -->

## Save, revert, erase and reset

- **Save to dongle** checks the map the way the dongle does, and lists every problem first, under *The dongle would refuse this map*. A threshold of 0, or Keep with no target, are examples. Save becomes available once they're fixed.
- **Revert** loads the stored map again and drops your unsaved edits.
- **Erase this profile** deletes the profile, and that controller model goes back to the default map.
- **Reset all maps** erases every profile and empties the default map. Settings and paired controllers are kept.
- **Edit another map:** choose it under **Map to edit** and click **Load**. You can choose the default map, the connected controller, a stored profile, or **New profile**. For a new profile, type a VID, PID and name, which lets you set up a controller that isn't connected. The **Profiles** card lists the stored profiles, each with an **Edit** button.

## Triggers

Each trigger reaches the host two ways, and either one pulls it:

- **The trigger axis** (*Left trigger* / *Right trigger*, 0 to 100 %). In Switch Pro mode ZL and ZR are on/off, and the axis presses them past 50 %. In Xbox 360 mode LT and RT follow the axis all the way, from released to fully pulled.
- **The L2 / R2 button.** In Switch Pro mode it presses ZL / ZR. In Xbox 360 mode it pulls LT / RT fully.

So a controller whose triggers are on/off buttons works with no rule at all, and a rule can target either one. Target L2 / R2 for a plain press. Target the trigger axis when you want part of a pull in Xbox 360 mode, for example a *button → axis* rule at +50 %.

> [!NOTE]
> Firmware v4.0.0 sent only the trigger axes, so on/off triggers and rules that target L2 / R2 did nothing on the host. [Update the firmware](updating.md) if that is what you see.

## Recipes

Each recipe gives the rules to add in the **Map editor**, using the editor's column names. Add them to the connected controller's profile, or to the default map to cover every controller without a profile, then click **Save to dongle**. A blank Keep means unticked.

### Nintendo button positions on the Switch

**For:** an Xbox-labelled controller on the Switch, if you'd rather have the buttons where a Pro Controller has them (confirm on the right, back on the bottom).

Add the **Swap South / East (Nintendo A/B)** and **Swap West / North** presets.

| Kind | Source | Target | Keep | Parameter |
|---|---|---|---|---|
| button → button | South | East | | — |
| button → button | East | South | | — |
| button → button | West | North | | — |
| button → button | North | West | | — |

The bottom button now sends B, the right one A, the left one Y and the top one X, the same as a Pro Controller. The profile applies in Xbox 360 mode too, so remove these rules before you play on a PC with the same controller.

### Left-handed: swap the sticks

**For:** moving with your right thumb and aiming with your left.

Add the **Swap sticks** preset. To swap the bumpers too, add the following. Swap the triggers the same way with two *axis → axis* rules (Left trigger to Right trigger, and back, at × 100 %).

| Kind | Source | Target | Keep | Parameter |
|---|---|---|---|---|
| button → button | L1 | R1 | | — |
| button → button | R1 | L1 | | — |

### Invert a stick

**For:** flight-style controls or an inverted camera.

For up and down, use the **Invert left Y** or **Invert right Y** preset. For left and right, add:

| Kind | Source | Target | Keep | Parameter |
|---|---|---|---|---|
| axis → axis | Left stick X | Left stick X | | × −100 % |

### Steady a drifting or shaky stick

**For:** a stick that creeps when you let go, or small unintended movements.

Raise **Left stick dead zone** or **Right stick dead zone** a few percent at a time. The dashed circle on the Raw stick shows the size as you drag. Save, let go of the stick, and check that the Mapped stick stays centered. Repeat until it does. A worn stick may need 10 to 20 %. The slider goes up to 90 %, which leaves only big, deliberate pushes. See [Dead zones](#dead-zones).

### More reach from a small movement

**For:** limited range of motion. The stick reaches full deflection before your thumb reaches the edge.

| Kind | Source | Target | Keep | Parameter |
|---|---|---|---|---|
| axis → axis | Left stick X | Left stick X | | × 200 % |
| axis → axis | Left stick Y | Left stick Y | | × 200 % |

At 200 % the stick reads full at half its travel, and at 400 % (the most) it reads full at a quarter. Each axis is scaled on its own and stops at full. Add a small dead zone as well, so a resting thumb doesn't nudge the stick.

### Click a stick without pressing it in

**For:** when pressing a stick in (often sprint or crouch) is hard or tiring. This swaps the left stick click with the left bumper.

| Kind | Source | Target | Keep | Parameter |
|---|---|---|---|---|
| button → button | L1 | L3 | | — |
| button → button | L3 | L1 | | — |

Use R1 and R3 for the right stick. If you don't need the bumper's own action, leave out the second rule. The bumper then clicks the stick, and the stick click keeps working as well.

### Camera on the d-pad (movement and camera with one hand)

**For:** playing with the left hand only. The left stick still moves you, and the d-pad turns the camera.

| Kind | Source | Target | Keep | Parameter |
|---|---|---|---|---|
| button → axis | D-pad Left | Right stick X | | −100 % of full scale |
| button → axis | D-pad Right | Right stick X | | +100 % of full scale |
| button → axis | D-pad Up | Right stick Y | | −100 % of full scale |
| button → axis | D-pad Down | Right stick Y | | +100 % of full scale |

The d-pad no longer works as a d-pad, and the right stick still works too. For slower turning, lower the amounts to 50 %. The same four-rule pattern puts a stick on any four buttons. On the face buttons, for example, it gives movement to the right hand.

### Stick as a d-pad

**For:** menus and games that only read the d-pad, or a d-pad that is hard to press.

For the left stick, add the **Left stick → D-pad** preset. For the right stick, add:

| Kind | Source | Target | Keep | Parameter |
|---|---|---|---|---|
| axis → button | Right stick X | D-pad Left | ✓ | left past (-) 50 % |
| axis → button | Right stick X | D-pad Right | ✓ | right past (+) 50 % |
| axis → button | Right stick Y | D-pad Up | ✓ | up past (-) 50 % |
| axis → button | Right stick Y | D-pad Down | ✓ | down past (+) 50 % |

With Keep ticked, the stick also still works as a stick.

### Put ZR or RT on a button

**For:** when pulling a trigger is hard. Pressing R1 pulls the right trigger fully.

| Kind | Source | Target | Keep | Parameter |
|---|---|---|---|---|
| button → axis | R1 | Right trigger | | +100 % of full scale |
| axis → button | Right trigger | R1 | | pressed past 50 % |

The first rule does the job. A *button → button* rule from R1 to R2 does the same ([Triggers](#triggers)). The second rule is optional: it puts R1's own action on the trigger, so the two swap. Use L1 and Left trigger for ZL / LT.

### Light-touch trigger

**For:** a trigger that should fire with a short or light pull.

| Kind | Source | Target | Keep | Parameter |
|---|---|---|---|---|
| axis → axis | Right trigger | Right trigger | | × 400 % |

At 400 % the trigger reads fully pulled at a quarter of its travel. In Switch Pro mode, ZR then presses at about an eighth of the travel. At 200 % the trigger reads full at half its travel.

### Use a trigger as a button

**For:** pressing a button by pulling a trigger, which can suit you better than the face buttons.

| Kind | Source | Target | Keep | Parameter |
|---|---|---|---|---|
| axis → button | Right trigger | South | | pressed past 30 % |

With Keep off, the trigger only presses South. Tick Keep if it should also still work as a trigger.

### Keep a trigger held without holding it

**For:** long holds, such as the accelerator in a racing game.

| Kind | Source | Target | Keep | Parameter |
|---|---|---|---|---|
| axis → axis | Right trigger | Right trigger | | × −100 % |

The trigger now rests fully pressed, and pulling it lets go. It also stays pressed in menus, so delete the rule when you're done with that game.

### One button for L + R

**For:** screens that ask you to press L and R together. One source can feed several targets, so this puts both on the Share (Capture) button.

| Kind | Source | Target | Keep | Parameter |
|---|---|---|---|---|
| button → button | Capture | L1 | | — |
| button → button | Capture | R1 | | — |

Share then no longer takes screenshots. On the Switch you may not need this for registering: the dongle already holds L + R by itself for one second each time the Switch connects to it over USB ([Nintendo Switch](hosts.md#nintendo-switch)).

### Stop pressing Home by accident

**For:** a Home or Xbox button that gets bumped mid-game.

| Kind | Source | Target | Keep | Parameter |
|---|---|---|---|---|
| button → button | Home | none (remove the source) | | — |
| button → button | Capture | Home | | — |

The first rule switches the Xbox button off. The second makes Share the Home button, so you can still reach the HOME menu on the Switch (or the Guide in Xbox 360 mode, where Share does nothing otherwise). Leave out the second rule to have no Home button at all.

## Limits

- **Profiles:** 16, plus the default map. The **Profiles** card shows how many are stored, for example *3 / 16*. To add one when the dongle is full, erase another first.
- **Rules:** 64 per map.
- **Dead zones:** 0 to 90 % per stick. Triggers have none.
- **Scale (axis → axis):** −400 to +400 %.
- **Threshold (axis → button):** 1 to 100 %. A trigger's threshold always points the *pressed* way.
- **Amount (button → axis):** −100 to +100 % of full travel.
- **Profiles follow the controller model,** not the individual controller, the game or the USB mode.

### What it can't do yet

- **No turbo or rapid fire, macros, or sequences of presses.**
- **No toggles or latching** (press once to hold). [Keep a trigger held without holding it](#keep-a-trigger-held-without-holding-it) is the closest it gets.
- **No combinations** where two buttons pressed together do something else, and **no shift layers.**
- **No switching maps from the controller, or per game.**
- **No export, import or backup of maps.**
- **No motion controls** ([What is forwarded](controllers.md#what-is-forwarded)).

## Coming from v3

Firmware before v4 had five input settings on the Settings tab: **Invert left stick Y**, **Invert right stick Y**, **Swap A / B**, **Swap X / Y** and **Stick deadzone**. The first time v4 starts with map storage (that is, after the [Full update](updating.md#full-update)), the dongle turns those settings into the default map, once:

- **Swap A / B** becomes the Swap South / East rules.
- **Swap X / Y** becomes the Swap West / North rules.
- **Stick deadzone** becomes the same dead zone on both sticks.
- **Invert left / right stick Y, switched off,** becomes an Invert rule for that stick. Switched on, as it was by default, it needs no rule, because v4 always sends stick Y the way the Switch and PCs expect.

If you never changed those settings, the default map stays empty. On v4 the console hides the old settings and says *Stick / button options moved to Button mapping*.

## Use espp's Gamepad Map Console

The dongle uses espp's gamepad map protocol, so espp's own [Gamepad Map Console](https://esp-cpp.github.io/espp/apps/gamepad_map_console.html) can edit its maps too. In that page, tick **Any device (ignore VID/PID filter)** before you click **Connect (WebUSB)**. Otherwise its device picker lists only devices with espp's default USB vendor ID, and the dongle isn't one of them. Only one page can use the dongle at a time, so disconnect the dongle console first.

[Console protocol notes](../web/README.md) has the protocol-level detail.

# Transformation Mask Port Notes

## Direct MM Translation Areas

- MM player asset paths are highly portable. Skeletons, display lists, texture layers, and `misc/link_animetion/gPlayerAnim_*` animation names can generally be mapped directly through the existing `mm.o2r` loader.
- Goron roll display assets, heat layers, spike layers, punch animations, sidehop/backflip starts and ends, climb pieces, door animation, mask-off start, and ocarina animations all have direct resource counterparts.
- Damage behavior can mostly follow MM intent once routed through OoT collider flags. Goron punch and roll use bomb-like damage flags without needing to spawn a real bomb/explosion actor.
- Form-specific restrictions such as Goron ledge-climb blocking, fire immunity, and water void-out are good candidates for direct behavioral mapping, with only state flag names adapted.

## Adapter Areas In Current SoH

- MM/2Ship drives the active player skeleton directly. This branch renders a separate transformation skeleton over the OoT `Player`, so root motion must be copied from the form animation into the OoT actor and then normalized on the custom skeleton.
- Persistent masks across load zones are adapter-sensitive. Runtime state must be tied to the current `PlayState`, `Player`, and scene, because the form skeleton/resources are static while OoT rebuilds the player actor around scene transitions.
- Punch chaining cannot be an instant animation restart. In this branch it is queued, then advanced at the punch/end boundary so root translation does not get reset mid-stride.
- Sidehop/backflip and other movement animations need explicit end-animation transitions. OoT's player state machine does not automatically know about the MM Goron animation split.
- Shielding uses OoT guard state detection, but the visible pose needs MM Goron guard animations or a dedicated Goron shield skeleton path.
- Audio from `mm.o2r` needs a carrier/bridge because OoT and MM sound ids are not interchangeable in this build. Keep that adapter separate from animation work.
- Ocarina/instrument display is mostly direct display-list attachment, but limb indices and instrument rotations are form-specific adapter data.

## Goron Parity Pass Notes

- Direct parity work is inferred from `mm_player_form.cpp` log lines, pinned `mm.o2r` assets, and in-game comparison.
- Keep this branch's rolling model rotation adapter. It avoids the ground clipping seen in the MM reference behavior's roll, and the user has confirmed it is currently better than the MM reference behavior's behavior.
- MM reference frame hints override the raw animation XML lengths for Goron climb, door, chest, and instrument animations. Several raw files are much longer than the frame windows used by the MM reference behavior.
- Goron heat/spike progression should be velocity-gated: sustained fast roll builds heat, sustained top-speed roll builds spikes. Total time spent curled should not be enough by itself.
- Ground slam should remain a curled/rolling action with delayed impact damage and quake on landing, not a repurposed punch-C pose.
- Roll visuals now include a small speed-based bounce/compression layer. Keep it subtle so the established roll-ground alignment does not regress.

## Reuse Guidance For Zora, Deku, And Fierce Deity

- First map the asset paths exactly, then add a thin adapter around OoT player state rather than rewriting the visible behavior from scratch.
- Treat animation root translation as suspicious until proven otherwise. If an animation visually drifts, decide whether the movement should move the OoT actor or be stripped from the rendered skeleton.
- Preserve MM animation start/end pairs whenever the MM reference behavior lists them separately; relying on a single start animation usually causes the visible pose to hang or snap.
- Keep special-form input rules separate from generic render override code so each form can be tuned without destabilizing the others.

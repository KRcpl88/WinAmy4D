# AI-Assisted Asset Generation Guide

## Overview

This document outlines the recommended approach for creating animated 3D holographic
chess piece assets using generative AI tools, targeting a Steam release with Godot 4
as the game engine.

## Design Decision: Anthropomorphic Humanoid Pieces

Rather than traditional chess piece shapes (pawn cylinder, rook tower, etc.), the pieces
are designed as **humanoid miniature characters**. This unlocks the full AI animation
ecosystem, since all mainstream AI animation tools target humanoid skeletons.

### Piece Archetypes

| Piece  | Character Concept                    |
|--------|--------------------------------------|
| Pawn   | Foot soldier / drone                 |
| Rook   | Heavy armored sentinel / golem       |
| Knight | Mounted warrior or agile swordsman   |
| Bishop | Robed mage / priest                  |
| Queen  | Sorceress / empress                  |
| King   | Armored monarch                      |

---

## Recommended Tools

### 3D Mesh Generation

**Meshy-6** (meshy.ai) — $12/month Pro plan

- Text/Image → 3D with dedicated Low Poly Mode for game-ready assets
- Direct `.blend` export (also .glb, .fbx, .obj)
- AI Texturing: generates PBR maps (Albedo, Roughness, Metallic, Normal) up to 4K
- Auto-rigging for humanoid characters in under 30 seconds
- Native Blender and Godot plugins
- Commercial rights on paid plans

**Alternative:** Tripo3D ($12/month) — competitive quality, cleaner topology on
simple objects.

### Animation

| Tool | Use Case | Notes |
|------|----------|-------|
| **Mixamo** (free, Adobe) | Idle, walk, attack, death clips | 2,500+ preset motions, auto-rig upload |
| **DeepMotion SayMotion** | Custom text-prompted animations | "warrior raises sword triumphantly" → animation |
| **Meshy Animation** | Integrated rig + 100+ presets | One-click if mesh generated in Meshy |
| **Cascadeur** | Physics-based combat/capture polish | AI-assisted posing with realistic weight |

All tools export FBX/GLB compatible with Blender and Godot.

### Textures & Materials

- **Meshy AI Texturing** — Sci-fi/holographic style prompts generate PBR texture sets
- **Adobe Substance 3D** (~$55/month) — Professional option for emissive + holographic
  material authoring

### Holographic Shader (Manual)

The holographic effect is a real-time shader, not a texture. AI cannot generate this
automatically. Key components:

- Fresnel rim glow (brighter at silhouette edges)
- Emissive color channel (cyan/blue/red team colors)
- Animated scanline pattern (scrolling via TIME uniform)
- Additive blending (eliminates transparency sorting issues)
- Optional dissolve effect for capture animations

---

## Production Pipeline

### Phase 1: Mesh Generation (~3 hours)

```
Meshy-6 Pro → Text prompt per piece type:
  "holographic sci-fi [piece archetype] character, translucent blue
   glowing energy, futuristic armor, T-pose, low poly, game ready"

Generate 6 piece types × ~3 iterations each = ~18 generations
Budget: ~270 credits of 3,000/month allotment
```

### Phase 2: Auto-Rig (~30 minutes)

- Use Meshy's built-in auto-rig, or
- Upload to Mixamo for automatic humanoid skeleton

### Phase 3: AI Animation (~2 hours)

Pull animation clips from Mixamo or generate via DeepMotion SayMotion:

| Animation | Description | Frames (60fps) |
|-----------|-------------|-----------------|
| Idle      | Subtle breathing, hover sway | 120 (looping) |
| Move      | Walk/glide to new square | 30–45 |
| Attack    | Sword swing, spell cast | 30–45 |
| Captured  | Collapse, dissolve reaction | 30–60 |

### Phase 4: Blender Refinement (~8 hours)

1. Import rigged + animated meshes
2. Check topology, run Decimate if needed
3. Adjust animation timing and transitions
4. Set up holographic shader material nodes
5. Add particle effects for capture dissolve (Geometry Nodes)
6. Create color variant for Team 2 (swap emission color)

### Phase 5: Export for Godot (~2 hours)

```
Export each piece as .glb with embedded:
  - Skeleton + skin weights
  - Animation clips: Idle, Move, Attack, Captured
  - Material references (shader applied in Godot)
```

---

## Time & Cost Estimate

| Phase | Time | Cost |
|-------|------|------|
| Mesh generation (6 pieces) | 3 hours | ~$12/month (Meshy Pro) |
| Auto-rigging | 30 min | Free (Mixamo) or included |
| AI animation clips | 2 hours | Free (Mixamo) |
| Blender refinement + shader | 8 hours | Free |
| Animation polish | 4 hours | Free |
| Color variants (Team 2) | 1 hour | Free |
| Godot integration | 2 hours | Free |
| **Total** | **~20 hours** | **~$12–50** |

**Compare to fully manual pipeline: 100–160+ hours.**

---

## Licensing & Steam Requirements

### Asset Ownership

- **Meshy Pro/Max:** Full commercial rights. You own generated assets.
- **Mixamo:** Free for commercial use (Adobe account required).
- **Tripo3D Pro:** Commercial rights on paid plans.

### Steam AI Content Policy

Valve allows AI-generated content on Steam. Requirements:

1. Disclose AI-generated content during app submission (Steam partner form)
2. Ensure no copyrighted training data reproduced (low risk for geometric game pieces)
3. AI-generated content does not automatically disqualify a game

### Legal Risk Assessment

Chess piece characters are generic fantasy archetypes (warrior, mage, knight). Copyright
risk from AI training data is extremely low for these concepts. Unique stylization
(holographic aesthetic, custom shader) further differentiates from any training source.

---

## Technology Stack Summary

```
WinAmy4D (C++ engine) ──GDExtension──→ Godot 4 (game shell)
                                           │
                                           ├── 3D Scene (holographic board + pieces)
                                           ├── AnimationTree (StateMachine per piece)
                                           ├── Custom Spatial Shader (hologram effect)
                                           └── GodotSteam (Steamworks integration)

Asset Pipeline:
  Meshy-6 → Blender (cleanup + shader) → .glb → Godot import
  Mixamo  → Blender (timing adjust)    → embedded in .glb
```

---

## References

- Meshy-6: https://www.meshy.ai/
- Mixamo: https://www.mixamo.com/
- DeepMotion SayMotion: https://www.deepmotion.com/saymotion
- Cascadeur: https://cascadeur.com/
- Tripo3D: https://tripo3d.ai/
- Godot GDExtension: https://docs.godotengine.org/en/stable/tutorials/scripting/cpp/gdextension_cpp_example.html
- GodotSteam: https://codeberg.org/godotsteam/godotsteam
- Steam AI Policy: Disclosed via Steamworks partner submission form

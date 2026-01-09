# 3016CW2
youtube：https://www.youtube.com/watch?v=wyX4OB6QS3c
CW2 OpenGL First-Person Jumping Platform Game (Sandbox Scenario):
This is a first-person jumping platform game developed using C++ and OpenGL 4.3 Core. The project includes procedural terrain, grass/rock/snow terrain texture blending, platforms, scoring UI, skybox, and irrKlang audio playback.
1. Rendering / Graphics:
-MVP (Model / View / Projection) GLM Implementation:
Phong lighting model implementation
Terrain: The game has loaded three basic textures, each corresponding to a different terrain layer: Unit 0 (Grassland): `grass.jpg`, Unit 1 (Rock): `rock.png`, and Unit 2 (Snowfield): `snow.png`
On the mixed equation, a sector region division based on polar coordinate angles is employed. The distance between the current angle and the central angle of each biome is calculated as follows: float grassDist = std::min(abs(angleDeg - 270.0f), 360.0f - abs(angleDeg - 270.0f))
Skybox (Cubemap): - Use the notation `gl_Position = pos.xyww` to ensure that the skybox is always at the farthest distance
Upper left quadrilateral score display
The signature in the lower right corner is displayed as a quadrilateral
2. Gameplay / Interaction
- **First-person movement**: WASD + mouse for camera control, right-click to accelerate, and spacebar to jump
- Fly Mode toggle (press `F`), press space/e to ascend, q to descend
- **Collection system**: Rotate the hemisphere, approach to collect points and play sound effects
- `ESC`: Exit the program
3. Scene animation
- Rotating platform: Continuously rotates around the Y axis
- Mobile platform: sinusoidal reciprocating movement
- Collection: Continuous spinning animation

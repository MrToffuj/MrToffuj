# Preview renderer

Renders a frame of the real simulation with three.js, using the same placeholder look as the Unreal project.

```bash
cmake --build build --target fightcore_snapshot
./build/fightcore_snapshot 2 1200 7 > brawl.json        # mode, frames, seed [, map 0-5]
npm install three@0.160.0 playwright
node shot.js '[["brawl.json","chase","brawl_chase.png"],["brawl.json","aerial","brawl_aerial.png"]]'
```

#include <cels/cels.h>
#include <cels_sdl3.h>

CEL_Compose(World) {
    SDL3Context(.video = true) {}
}

cels_main() {
    cels_register(SDL3_Engine);
    cels_session(World) {
        // SDL3 is now initialized via lifecycle observer
        // Run a few frames to prove the loop works, then exit
        int frames = 0;
        while (cels_running() && frames < 5) {
            cels_step(0.016f);
            frames++;
        }
        // SDL3 shuts down automatically when World entity is destroyed
    }
}

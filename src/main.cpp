#include <chrono>
#include <thread>

#include "main.h"
#include "cpu.h"
#include "interface.h"
#include "rom.h"

#include "ppu.h"
#include "memory.h"

// Main code
int main(int, char**)
{
    load_rom("roms/Tetris (World) (Rev A).gb");

    start_interface();

    write_byte(0xFF46, 0x03);
    // Main loop
    for (;;)
    {
        //execute_cycle();
        if (update_interface() == 1)
            break;

        std::this_thread::sleep_for(std::chrono::milliseconds(16));
    }

    stop_interface();

    return 0;
}

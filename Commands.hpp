// Commands.hpp
#pragma once

class NeReLaBasic; // Forward declaration to avoid circular dependencies

namespace Commands {
    void do_print(NeReLaBasic& vm);
    void do_let(NeReLaBasic& vm);
    void do_goto(NeReLaBasic& vm);
    void do_if(NeReLaBasic& vm);
    void do_else(NeReLaBasic& vm);

    void do_list(NeReLaBasic& vm);
    void do_load(NeReLaBasic& vm);
    void do_save(NeReLaBasic& vm);
    void do_run(NeReLaBasic& vm);
    // We will add do_let(), do_if(), do_goto(), etc. here later.
}
import time
import curses
from curses import wrapper
import asyncio
from asyncio.subprocess import PIPE
import sys
import signal

ON_POSIX = 'posix' in sys.builtin_module_names

full_block = '█'
vline = '┃'
hline = '━'
tup = '┻'
minibuffer = ''
minibuffer_history = []


def default_process(key):
    global minibuffer
    minibuffer += key.__repr__()

def add_to_history(text):
    global minibuffer_history
    if len(minibuffer) > 20:
        minibuffer_history.pop(0)
    minibuffer_history.append(text)


def main(stdscr):
    global minibuffer
    global minibuffer_history
    do_quit = False

    while not do_quit:
        # Clear screen
        stdscr.clear()
        max_lines, max_cols = curses.LINES, curses.COLS

        # Divides the screen in a bottom and a top
        half_lines = max_lines // 2
        half_cols = max_cols // 2

        for i in range(0, half_lines):
            stdscr.addstr(i, half_cols, vline)

        stdscr.addstr(half_lines, 0, hline * (max_cols - 1))
        stdscr.addstr(half_lines, half_cols, tup)

        stdscr.addstr(max_lines - 2, 0, hline * (max_cols - 1))
        stdscr.move(max_lines - 1, 0)
        stdscr.addstr(minibuffer)
        stdscr.refresh()

        key = stdscr.getkey()

        if key is None:
            pass

        elif key in ['KEY_BACKSPACE', '\b']:
            minibuffer = minibuffer[:-1]

        # Special single-stroke commands
        elif minibuffer == '':
            if key.lower() == 'q':
                do_quit = True
            else:
                default_process(key)
        else:
            default_process(key)

#wrapper(main)



async def sameboy():
    p = await asyncio.create_subprocess_shell('sameboy main.gb', stdin=PIPE, stdout=PIPE, bufsize=0, close_fds=ON_POSIX)
    print(p.send_signal(signal.SIGINT))

    print("Waiting to start...")
    time.sleep(4)


    # ... do other things here

    do_quit = False
    last_time = time.time()
    await p.stdin.drain()
    print(p.stdin.write(b'\x03\nreg\n'))
    await p.stdin.drain()
    future_line = asyncio.create_task(p.stdout.readline())
    try:
        while do_quit is False:
            # Wait for line
            if not future_line.done():
                if (time.time() - last_time) > 0.1:
                    last_time = time.time()
                    print('no output yet')
                    await p.stdin.drain()
                    print(p.stdin.write(b'reg\n'))
                    await p.stdin.drain()
                    future_line.cancel()
                    try:
                        await future_line
                    except asyncio.CancelledError:
                        #print(await p.stdout.readline())
                        future_line = asyncio.create_task(p.stdout.readline())
            else:
                # got line
                line = future_line.result()
                print(line.__repr__())
                await p.stdin.drain()
                print(p.stdin.write(b'reg\n'))
                await p.stdin.drain()
                future_line = asyncio.create_task(p.stdout.readline())
    finally:
        p.kill()

asyncio.run(sameboy())

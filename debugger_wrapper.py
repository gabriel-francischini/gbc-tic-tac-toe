import time
import curses
from curses import wrapper
import sys
from subprocess import PIPE, Popen
from threading  import Thread
from queue import Queue, Empty


ON_POSIX = 'posix' in sys.builtin_module_names


def enqueue_output(out, queue):
    for line in iter(out.readline, b''):
        queue.put(line)
        print(line)
        time.sleep(0.1)
    out.close()


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






p = Popen(['sameboy', 'main.gb'], stdin=PIPE, stdout=PIPE, bufsize=0, close_fds=ON_POSIX)
q = Queue()
t = Thread(target=enqueue_output, args=(p.stdout, q))
t.daemon = True # thread dies with the program
t.start()

# ... do other things here

do_quit = False
last_time = time.time()
try:
    while do_quit is False:
        try:
            # read line without blocking
            line = q.get_nowait() # or q.get(timeout=.1)
        except Empty:
            if (time.time() - last_time) > 0.5:
                last_time = time.time()
                print('no output yet')
                if p.poll() is None:
                    print(p.stdin.write(b'reg\n'))
        else:
            # got line
            print(line.__repr__())
            # ... do something with line
finally:
    p.kill()

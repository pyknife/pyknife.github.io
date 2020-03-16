import locale
locale.setlocale(locale.LC_ALL, '')
code = locale.getpreferredencoding()
import curses


stdscr = curses.initscr()
.resizeterm(60,80)
stdscr.addstr(10,20,"current mode: 123321")
stdscr.refresh()
y, x = stdscr.getmaxyx()
stdscr.getch()
p = curses.KEY_RESIZE
stdscr.getch()
curses.endwin()

print(y,x)

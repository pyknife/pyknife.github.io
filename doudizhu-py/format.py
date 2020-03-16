#This stuff will format cards to let cardtype not to crash.
#number appeared will be compared first, if same, compare number.
#this is not that readable, man...

import functools

def appearTime(cards):
    time = {}
    for foo in cards:
        time[foo] = time[foo] + 1 if foo in time else 1
    return time

def formatter(cards):
    time = appearTime(cards)
    return sorted(cards,
            key = functools.cmp_to_key(lambda x,y:(0 if x == y else
                                                  +1 if x > y else
                                                  -1 if x < y else 0) if time[x] == time[y] else(
                                                            -1 if time[x] > time[y] else
                                                            +1 if time[x] < time[y] else 0)))
#IT WORKED!!!

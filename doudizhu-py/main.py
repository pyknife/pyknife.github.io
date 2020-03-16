from ai import *
from board import *
from card import *
from cardtype import *
from format import *
from legal import *
from player import *
from sond import *

SYSTYPE = platform.system()
#Convert JQKA2 into 12345 stuff 
def calcPoint(winPlayer,player,allpoint):
    addAmount = 1.0 if winPlayer.isLandlord else 0.5
    addOrMinus =1 if winPlayer.isLandlord == player.isLandlord else -1
    player.score += int(allpoint * addAmount * addOrMinus)
def recognizable(lst):
        _dict = {'8': 8, '10': 10, '6': 6, '9': 9, '7': 7, '5': 5, '3': 3, '4': 4,'J':11,'Q':12,'K':13,'A':14,'2':15,'joker':16,'JOKER':17,'j':11,'q':12,'k':13,'a':14}
        res = lst[:]
        if len([x for x in res if not x in _dict]) != 0:
            return [3,4] #just a invaild handing
        return [_dict[x] for x in res]

def substr(strlst,x,y,string):
    for foo in range(x,x+len(string)):
        strlst[y] = subst(strlst[y],foo,string[foo-x])

os.system("cls") if SYSTYPE == "Windows" else os.system("clear")
process = play("BGM.wav")
print(""" 
             _   _                 
 _ __  _   _| |_| |__   ___  _ __  
| '_ \| | | | __| '_ \ / _ \| '_ \ 
| |_) | |_| | |_| | | | (_) | | | |
| .__/ \__, |\__|_| |_|\___/|_| |_|
|_|    |___/                       
     _                 _ _     _           
  __| | ___  _   _  __| (_)___| |__  _   _ 
 / _` |/ _ \| | | |/ _` | |_  / '_ \| | | |
| (_| | (_) | |_| | (_| | |/ /| | | | |_| |
 \__,_|\___/ \__,_|\__,_|_/___|_| |_|\__,_|
                                           
欢迎来到斗地主
本程序由公众号：早起python 整理
请勿用于任何商业用途。
""")
while True:
        people = input("请输入你想几个人玩")
        if people == "license":
            print("""
本程序由Frank Blackburn and LeBron Jones制作，公众号：早起python 整理

Copyright (C) [2017] [Frank Blackburn and LeBron Jones]

""")
        elif '1'<=people<= '3':
            break

BOARDHEIGHT = 25
BOARDLENGTH = 65
card = card()
landlordCard = poker() #to display color
handingstr = 0
handing = []
handingWcolor = poker()
previousHandingWcolor = poker()
previousHanding = []
previousHandingPlayer = 0
lastHumanPlayer = 0
py1 = 0
py2 = 0
py3 = 0
currentplayer = py1
boa =board()
#landlord
#now assign player.
#I can't find a way to abstract this stuff. I'll just write hard codes.

Flag = True
if people == '3':
    while Flag:
            print("Shuffling...")
            card.shuffle()
            py1 = player(False,py3,py2,card.handOut(0))
            py2 = player(False,py1,py3,card.handOut(1))
            py3 = player(False,py2,py1,card.handOut(2))
            py1.resetPlayer(py3,py2)
            py2.resetPlayer(py1,py3)
            py3.resetPlayer(py2,py1)
            py1.cards.sort()
            py2.cards.sort()
            py3.cards.sort()
            currentplayer = py1
            lastHumanPlayer = py1
            for count in range(1,4):
                    boa.draw(lastHumanPlayer,lastHumanPlayer.previousPlayer,lastHumanPlayer.nextPlayer,[],BOARDLENGTH,BOARDHEIGHT)
                    p = input("Player"+str(count)+" 你想抢地主吗(Y/n)")
                    if p.lower() == 'y':
                            currentplayer.isLandlord = True
                            currentplayer.cards.addCard(card.handOut(3))
                            lastHumanPlayer = currentplayer
                            Flag = False
                            break
                            #reshuffle if nobody wanna be landlord
                    else:
                            currentplayer = currentplayer.nextPlayer
                            #this is like a linked list

if people == '2':
    while Flag:
            print("Shuffling...")
            card.shuffle()
            py1 = player(False,py3,py2,card.handOut(0))
            py2 = player(False,py1,py3,card.handOut(1))
            py3 = aiPlayer(False,py2,py1,card.handOut(2))
            py1.resetPlayer(py3,py2)
            py2.resetPlayer(py1,py3)
            py3.resetPlayer(py2,py1)
            py1.cards.sort()
            py2.cards.sort()
            py3.cards.sort()
            currentplayer = py1
            lastHumanPlayer = py1
            for count in range(1,4):
                    boa.draw(lastHumanPlayer,lastHumanPlayer.previousPlayer,lastHumanPlayer.nextPlayer,[],BOARDLENGTH,BOARDHEIGHT)
                    if (isinstance(currentplayer,aiPlayer)):
                        p = currentplayer.beLandlord()
                    else:
                        p = input("Player"+str(count)+" 你想抢地主吗?(Y/n)")
                        lastHumanPlayer = currentplayer
                    if p.lower() == 'y':
                            currentplayer.isLandlord = True
                            currentplayer.cards.addCard(card.handOut(3))
                            Flag = False
                            break
                            #reshuffle if nobody wanna be landlord
                    else:
                            currentplayer = currentplayer.nextPlayer
                            #this is like a linked list


if people == '1':
    while Flag:
            print("Shuffling...")
            card.shuffle()
            py1 = player(False,py3,py2,card.handOut(0))
            py2 = aiPlayer(False,py1,py3,card.handOut(1))
            py3 = aiPlayer(False,py2,py1,card.handOut(2))
            py1.resetPlayer(py3,py2)
            py2.resetPlayer(py1,py3)
            py3.resetPlayer(py2,py1)
            py1.cards.sort()
            py2.cards.sort()
            py3.cards.sort()
            currentplayer = py1
            lastHumanPlayer = py1
            for count in range(1,4):
                    boa.draw(lastHumanPlayer,lastHumanPlayer.previousPlayer,lastHumanPlayer.nextPlayer,[],BOARDLENGTH,BOARDHEIGHT)
                    print("DEBUG",isinstance(currentplayer,aiPlayer))
                    if (isinstance(currentplayer,aiPlayer)):
                        p = currentplayer.beLandlord()
                    else:
                        p = input("Player"+str(count)+" 你想抢地主吗?(Y/n)")
                        lastHumanPlayer = currentplayer
                    if p.lower() == 'y':
                            currentplayer.isLandlord = True
                            currentplayer.cards.addCard(card.handOut(3))
                            Flag = False
                            break
                            #reshuffle if nobody wanna be landlord
                    else:
                            currentplayer = currentplayer.nextPlayer
                            #this is like a linked list



print("底牌: ")
landlordCard.addCard(card.handOut(3))
landlordCard.sort()
print(landlordCard.displable())
input()
currentplayer.cards.sort()
boa.draw(lastHumanPlayer,lastHumanPlayer.previousPlayer,lastHumanPlayer.nextPlayer,[],BOARDLENGTH,BOARDHEIGHT)
previousHandingPlayer = currentplayer
#play!
while(not len(currentplayer.previousPlayer.cards) == 0):
        os.system("cls") if SYSTYPE == "Windows" else os.system("clear")#Not supporting non-POSIX
        if previousHanding != [] and previousHandingPlayer == currentplayer.previousPlayer:
            if cardtype(previousHanding)[0] == 'bomb':
                boa.baseMul(2)
            if cardtype(previousHanding)[0] == 'rocket':
                boa.baseMul(4)
        boa.draw(lastHumanPlayer,lastHumanPlayer.previousPlayer,lastHumanPlayer.nextPlayer,previousHandingWcolor.displable(),BOARDLENGTH,BOARDHEIGHT)
        if isinstance(currentplayer,aiPlayer):
            handingstr = letterCard(expand(currentplayer.pickCard(previousHandingPlayer,previousHanding,currentplayer.nextPlayer)))
            input()
        else:
            handingstr = input("请输入你想出的牌，并将每张牌以空格分开")
            lastHumanPlayer = currentplayer
        handing = formatter(recognizable(handingstr.split()))
        os.system("cls") if SYSTYPE == "Windows" else os.system("clear")
        handingWcolor.cards = currentplayer.cards.autoMatch(handing)
        if previousHandingPlayer == currentplayer:
            previousHanding = []
        if len(handing) == 0:#not handing anything
            if previousHandingPlayer != currentplayer:
                #boa.easydraw(currentplayer,currentplayer.previousPlayer,currentplayer.nextPlayer,["pass"])
                input("不是您的回合，请按下 ENTER .")
                currentplayer = currentplayer.nextPlayer
            else:
                input("必须按下回车继续游戏.")
        elif previousHanding == []:#first handing player
            if cardtype(handing) != -1 and handingWcolor.cards != -1:
                #boa.easydraw(currentplayer,currentplayer.previousPlayer,currentplayer.nextPlayer,handingWcolor.displable())
                currentplayer.cards.delCard(handingWcolor.cards)
                previousHandingPlayer = currentplayer
                previousHanding = handing
                previousHandingWcolor.cards = copy.deepcopy(handingWcolor.cards)
                currentplayer = currentplayer.nextPlayer
                os.system("cls") if SYSTYPE == "Windows" else os.system("clear")
                input("不是您的回合，请按下 ENTER.")
            else:
                input("错误! 请按下回车.")
        else:#This can only be follow-up
            if cardtype(handing)!= -1 and legal(previousHanding,handing) and handingWcolor.cards != -1:
                #boa.easydraw(currentplayer,currentplayer.previousPlayer,currentplayer.nextPlayer,handingWcolor.displable())
                currentplayer.cards.delCard(handingWcolor.cards)
                previousHandingPlayer = currentplayer
                previousHandingWcolor.cards = copy.deepcopy(handingWcolor.cards)
                previousHanding = handing
                currentplayer = currentplayer.nextPlayer
                os.system("cls") if SYSTYPE == "Windows" else os.system("clear")
                input("请按下回车让下一位用户出牌.")
            else:
                input("请按下回车.")
calcPoint(currentplayer.previousPlayer,currentplayer.previousPlayer,board.basepoint * board.dup)
calcPoint(currentplayer.previousPlayer,currentplayer,board.basepoint * board.dup)
calcPoint(currentplayer.previousPlayer,currentplayer.nextPlayer,board.basepoint * board.dup)
print("你的上家得分:",currentplayer.previousPlayer.score)
print("你的得分是:",currentplayer.score)
print("你的下家得分是:",currentplayer.nextPlayer.score)
stop(process)

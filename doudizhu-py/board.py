#import locale
#locale.setlocale(locale.LC_ALL, '')
#code = locale.getpreferredencoding()
#import curses
#NOT USING THESE F*****G CRAP ANYMORE

from player import *
from ai import *
from card import *

def make(length,height):
    strlst = []
    for foo in range(0,height):
        strlst.append("")
        for bar in range(0,length):
            strlst[foo] += " "
    return strlst

def subst(str,index,target):
    return str[:index] + target + str[index+1:]
def substr(strlst,x,y,string):
    for foo in range(x,x+len(string)):
        strlst[y] = subst(strlst[y],foo,string[foo-x])


def substituteRec(strlst,x,y,length,height,clearInside):
    for foo in range(x+1,x+length-1):
        strlst[y] = subst(strlst[y],foo,'─')
        strlst[y+height-1] = subst(strlst[y+height-1],foo,'─')
    for foo in range(y+1,y+height-1):
        strlst[foo] = subst(strlst[foo],x,'│')
        strlst[foo] = subst(strlst[foo],x+length-1,'│')
    
    strlst[y] =subst(strlst[y],x, '┌')
    strlst[y] =subst(strlst[y],x+length-1, '┐')
    strlst[y+height-1] = subst(strlst[y+height-1],x,'└')
    strlst[y+height-1] = subst(strlst[y+height-1],x+length-1,'┘')
    if clearInside == True:
        for foo in range(x+1,x+length-2):
            for bar in range(y+1,y+height-1):
                strlst[bar] = subst(strlst[bar],foo,' ')
def addNum(strlst,x,y,length,height,num): 
    strlst[y+int((height/2))] = subst(strlst[y+int(height/2)],x+int(length/2)-1,str(int(num / 10)))
    strlst[y+int((height/2))] = subst(strlst[y+int(height/2)],x+int(length/2)+1,str(int(num % 10)))
def addColor(strlst,x,y,cardWcolor):
    if cardWcolor[0] == 'joker':  #Some change to make print work
        cardWcolor[0] = 'jo'
        cardWcolor[1] = 'ker'
    elif cardWcolor[0] == 'JOKER':  #Some change to make print work
        cardWcolor[0] = 'JO'
        cardWcolor[1] = 'KER'
    substr(strlst,x+1,y+1,cardWcolor[0])
    substr(strlst,x+1,y+2,cardWcolor[1])

def printstr(strlst,height):
    for foo in range(0,height):
        print(strlst[foo])

class board(object):
        basepoint = 150
        dup = 1        
        def baseChange(self,new):
            self.basepoint = new

        def baseMul(self,base):
            self.dup *= base

        def easydraw(self,py,opp1,opp2,handing):
                print("opp1 "+str(len(opp1.cards)))
                print("opp2 "+str(len(opp2.cards)))
                print(handing)
                print(py.cards.displable())

        def draw(self,py,prevP,nextP,handing,length,height):
            CARDH = 7
            CARDW = 5
            x = 0
            y = 0
            p = []
            p = make(length,height)
            x = int(length/10)
            y = int(height/10)
            substituteRec(p,x,y,CARDH,CARDW,False)
            substituteRec(p,7*x,y,CARDH,CARDW,False)
            addNum(p,x,y,CARDH,CARDW,len(prevP.cards))
            addNum(p,7*x,y,CARDH,CARDW,len(nextP.cards))
            if(prevP.isLandlord):
                substr(p,x,y+CARDH-1,"landlord")
            else:
                substr(p,x,y+CARDH-1,"farmer")
            if(nextP.isLandlord):
                substr(p,7*x,y+CARDH-1,"landlord")
            else:
                substr(p,7*x,y+CARDH-1,"farmer")
            if(isinstance(prevP,aiPlayer)):
                substr(p,x+8,y+CARDH-1,"(ai)")
            if(isinstance(nextP,aiPlayer)):
                substr(p,7*x+8,y+CARDH-1,"(ai)")
            
            
            for foo in range(0,len(handing)):
                substituteRec(p,foo*3,y*5,CARDH,CARDW,True)
                addColor(p,foo*3,y*5,handing[foo])
            
            for foo in range(0,len(py.cards)):
                substituteRec(p,foo*3,y*8,CARDH,CARDW,True)
                addColor(p,foo*3,y*8,py.cards.displable()[foo])
            printstr(p,height)
            print("POINT:",self.basepoint*self.dup)



#card = card()
#boa = board()
#py1 = 0
#py2 = 0
#py3 = 0
#py1 = player(True,py3,py2,card.handOut(0))
#py2 = player(False,py1,py3,card.handOut(1))
#py3 = player(False,py2,py1,card.handOut(2))
#py1.resetPlayer(py3,py2)
#py2.resetPlayer(py1,py3)
#py3.resetPlayer(py2,py1)
#boa.draw(py3,py1,py2,[['6', '♦'], ['6', '♣']],60,25)

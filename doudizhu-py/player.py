import copy 
from card import poker
#use deep copy
#Portablity
class player(object):
    def __init__(self,_isLandlord,_previousPlayer,_nextPlayer,_initCards):
        self.cards = poker()
        self.isLandlord = _isLandlord
        self.previousPlayer = _previousPlayer
        self.nextPlayer = _nextPlayer
        self.cards.addCard(_initCards)
        self.score = 0
    def resetPlayer(self,_previousPlayer,_nextPlayer):
        self.previousPlayer = _previousPlayer
        self.nextPlayer = _nextPlayer


#test:
#dizhu = 0
#nong1 = 0
#nong2 = 0
#cards = [[4, 1], [5, 2], [6, 2], [6, 3], [9, 0], [11, 0], [13, 0], [13, 3], [15, 1], [15, 2], [15, 3], [16, 4], [3, 1], [11, 3], [10, 3], [14, 2], [11, 1]]
#dizhu = player(True, nong1, nong2, cards)



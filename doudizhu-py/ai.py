from player import *
from cardtype import *
from format import * 
class aiPlayer(player):
    def __init__(self,_isLandlord,_previousPlayer,_nextPlayer,_initCards):
        player.__init__(self,_isLandlord,_previousPlayer,_nextPlayer,_initCards)

    def analyzer(self):
        cardUncolor = {}
        cardSet = {"bomb":[],"triple":[],"pair":[],"single":[],"joker":[]}
        # 3+1, full house and 3+2, 4+2 will only be considered 
        complexCardSet = {"straight":[],"straight2":[],"airplane":[]}
        #delete colors of cards
        for foo in self.cards.cards:
            if foo[0] in cardUncolor:
                cardUncolor[foo[0]] += 1
            else:
                cardUncolor[foo[0]] = 1

        #find how many cards for each are there
        for foo in range(3,16):
            if foo in cardUncolor:
                if cardUncolor[foo] == 4:
                    cardSet["bomb"].append(foo)
                if cardUncolor[foo] == 1:
                    cardSet["single"].append(foo)
                if cardUncolor[foo] == 2:
                    cardSet["pair"].append(foo)
                if cardUncolor[foo] == 3:
                    cardSet["triple"].append(foo)
        if 16 in cardUncolor:
            cardSet["joker"].append(16)
        if 17 in cardUncolor:
            cardSet["joker"].append(17)

        #only find longest straight: have [3456789] wont get [34567].
        start = 3
        number = 0
        for foo in range(3,16):
            if foo in cardSet["triple"]: 
                number += 1
            else: # is not straight anymore
                if number >= 2: #can be airplane
                    complexCardSet["airplane"].append((start,number))
                start = foo + 1
                number = 0

        if number >= 2: #can be airplane
            complexCardSet["airplane"].append((start,number))

        #the same for double - but they can still hand double if they have triple
        start = 3
        number = 0
        for foo in range(3,16):
            if foo in cardSet["triple"] or foo in cardSet["pair"]: 
                number += 1
            else: # is not straight anymore
                if number >= 3: #can be straight2
                    complexCardSet["straight2"].append((start,number))
                start = foo + 1
                number = 0
        if number >= 3: #can be straight2
            complexCardSet["straight2"].append((start,number))
 
        #once again!
        start = 3
        number = 0
        for foo in range(3,16):
            if foo in cardSet["triple"] or foo in cardSet["pair"] or foo in cardSet["single"]: 
                number += 1
            else: # is not straight anymore
                if number >= 5: #can be straight
                    complexCardSet["straight"].append((start,number))
                start = foo + 1
                number = 0
        if number >= 5: #can be straight
            complexCardSet["straight"].append((start,number))
 

        return cardSet, complexCardSet

    def beLandlord(self):
        if self.cards.cards[8][0] > 10:
            return 'y' #For implement. Should be True
        else:
            return 'n'
#strategy/explaination in chinese
#分析手牌
#先看是不是王炸
#看是不是炸弹,如果是对手的炸弹,能打掉就打掉
#再看是不是大于K,如果是队友出的,就过牌,否则就试着打掉,用炸弹
#如果对手手牌少于9张出大于K或者出的大于8张的飞机/顺子/连对有王炸/炸弹就用掉
#否则无脑跟 不用炸弹和虎
#如果跟对方牌并且对方小于9张,没有1,2张的牌就拆2,3张的牌打
#自己出牌lastcard应该是[]
#如果自己手牌小于5张,先出王炸,炸弹,最大牌
#再看下家手牌数量和阵营 同阵营出最小单/对 不同阵营出比他多的牌或者大牌
    def pickCard(self,hander,lastcard,nextPlayer):
        cmon, cplx = self.analyzer()
        #string and tuple
        #these two are boolean
        if lastcard != []:
            isOpponent = self.isLandlord != hander.isLandlord
            cardType, number = cardtype(formatter(lastcard))    
            isCmon = cardType in cmon
            isCplx = not isCmon
            if cardType == "rocket":
                return "pass"

            if cardType == "bomb" and isOpponent:
                if len(cmon["bomb"]) > 0:
                    for foo in cmon["bomb"]:
                        if foo > number[0]:
                            return ("bomb",(foo,))

            if not isCplx and number[0] > 12:
                if not isOpponent:
                    return "pass"
                else:
                    if len(cmon[cardType])>0:
                        for foo in cmon[cardType]:
                            if foo > number[0]:
                                return(cardType,(foo,))
                        if len(cmon["bomb"]) > 0:
                            return("bomb", (cmon["bomb"][0],))
                        elif len(hander.cards) < 10 and cmon["joker"] == [16,17]:
                            return("rocket",(16,17))

            if cardType == "single" and number[0] > 13:
                if len(cmon["joker"]) > 0:
                    return("joker",(cmon["joker"][0],))
            
            if isCplx and cardType in ["straight","straight2","airplane"]:
                if cplx[cardType] != []:
                    for foo in cplx[cardType]:
                        if foo[1] >= number[1] and foo[0] + foo[1] - number[1] > number[0]: 
                            #(3,6) is 345678, for(4,3):456, 6>4 then hand last portion
                            if foo[0] > number[0]:
                                return (cardType,(foo[0],number[1]))
                            else:
                                return (cardType,(foo[0]+foo[1]-number[1],number[1]))
                if isOpponent:
                    if len(cmon["bomb"]) > 0:
                        return("bomb", cmon["bomb"][0])
                    elif len(hander.cards) < 10 and cmon["joker"] == [16,17]:
                        return("rocket",(16,17))
                    else:
                        return "pass"

            elif isCplx and cardType in ["airplanewithsmall","airplanewithbig"]:
                if cplx["airplane"] != []:
                    for foo in cplx["airplane"]:
                        if foo[1] >= number[1] and foo[0] + foo[1] - number[1] > number[0]: 
                            if cardType == "airplanewithsmall":
                                if len(cmon["single"])>=number[1]:
                                    wing = []
                                    for i in range(0,number[1]):
                                        wing.append(cmon["single"][i])
                                    if foo[0] > number[0]:
                                        return (cardType,(foo[0],number[1],wing))
                                    else:
                                        return (cardType,(foo[0]+foo[1]-number[1],number[1],wing))                            
 
                            if cardType == "airplanewithbig":
                                if len(cmon["pair"])>=number[1]:
                                    wing = []
                                    for i in range(0,number[1]):
                                        wing.append(cmon["pair"][i])
                                        wing.append(cmon["pair"][i])
                                    if foo[0] > number[0]:
                                        return (cardType,(foo[0],number[1],wing))
                                    else:
                                        return (cardType,(foo[0]+foo[1]-number[1],number[1],wing))                            
                if isOpponent:
                    if len(cmon["bomb"]) > 0:
                        return("bomb", cmon["bomb"][0])
                    elif len(hander.cards) < 10 and cmon["joker"] == [16,17]:
                        return("rocket",(16,17))
                    else:
                        return "pass"

            if not isCplx:
                if len(cmon[cardType])>0:
                    for foo in cmon[cardType]:
                        if foo > number[0]:
                            return(cardType,(foo,))
                    return "pass"
            

            if cardType == "3+1":
                if len(cmon["triple"])>0 and len(cmon["single"])>0:
                    for foo in cmon["triple"]:
                        if foo > number[0]:
                            return(cardType,(foo,cmon["single"][0]))
                        return "pass"
                    
            if cardType == "fullhouse":
                if len(cmon["triple"])>0 and len(cmon["pair"])>0:
                    for foo in cmon["triple"]:
                        if foo > number[0]:
                            return(cardType,(foo,cmon["pair"][0]))
                        return "pass"

            if cardType == "4+2":
                if len(cmon["bomb"])>0 and number[0] > 10:
                    return ("bomb",cmon[bomb][0])
            return "pass"   
        else:
            isOpponent = self.isLandlord != nextPlayer.isLandlord
            selBg = lambda x:(x,(cmon[x][-1],)) if len(cmon[x])>0 else False
            selSm = lambda x:(x,(cmon[x][0],)) if len(cmon[x])>0 else False
            selSmCplx = lambda x:(x,cplx[x][0]) if len(cplx[x])>0 else False
            #TODO: use a list to abstract this whole lambda if I still have time before due.
            if(len(self.cards)<5):
                if len(cmon["bomb"]) > 0:
                    return("bomb",(cmon["bomb"][0],))
                if cmon["joker"] == [16,17]:
                    return("rocket",(16,17))
                if selBg("joker"): return selBg("joker")
                if selBg("triple"): return selBg("triple")
                if selBg("pair"): return selBg("pair")
                if selBg("single"): return selBg("single")
            elif not isOpponent and len(nextPlayer.cards) < 4:
                if selSm("single"): return selSm("single")
                if selSm("pair"): return selSm("pair")
                if selSm("triple"): return selSm("triple")
                if selSm("bomb"): return selSm("bomb")
                if selSm("joker"): return selSm("joker")
            elif isOpponent and len(nextPlayer.cards) < 4:
                if selBg("bomb"): return selBg("bomb")
                if selBg("triple"): return selBg("triple")
                if selBg("pair"): return selBg("pair")
                if selBg("single"): return selBg("single")
            else:
                if selSmCplx("straight"): return selSmCplx("straight")
                if selSmCplx("straight2"): return selSmCplx("straight2")
                if selSmCplx("airplane"): return selSmCplx("airplane")
                if selSm("single"): return selSm("single")
                if selSm("pair"): return selSm("pair")
                if selSm("triple"): return selSm("triple")
                if selSm("joker"): return selSm("joker")
                if selSm("bomb"): return selSm("bomb")

def expand(pair):
    res = []
    _helper = {"single":1,"pair":2,"triple":3,"fullhouse":3,"3+1":3,"3+2":3,"straight":1,"straight2":2,"airplane":3,"joker":1,"bomb":4,"airplanewithsmall":3,"airplanewithbig":3}
    cmon = ["single","pair","triple","joker","bomb"]
    cplx = ["straight","straight2","airplane"]
    plane = ["airplanewithbig","airplanewithsmall"]
    if pair == "pass":
        return []
    if pair[0] in cmon:
        for i in range(0,_helper[pair[0]]):
            res.append(pair[1][0])
    elif pair[0] in cplx:
        for foo in range(pair[1][0],pair[1][0]+pair[1][1]):
            for i in range(0,_helper[pair[0]]): 
                res.append(foo)
    elif pair[0] in plane:
        for foo in range(pair[1][0],pair[1][0]+pair[1][1]):
            for i in range(0,3):
                res.append(foo)
        res += pair[1][2]
    else:
        for i in range(0,_helper[pair[0]]):
            res.append(pair[1][0])
        res += pair[1][1]
    return res

def letterCard(lst):
    helper = {3: '3', 4: '4', 5: '5', 6: '6', 7: '7', 8: '8', 9: '9', 10: '10', 11: 'J', 12: 'Q', 13: 'K', 14: 'A', 15: '2', 16: 'joker', 17: 'JOKER'}
    if lst == []:
        return ""
    else:
        return " ".join([helper[x] for x in lst])

#Test
py1 = 0
py2 = 0
ai = 0
py1 = aiPlayer(False,py2,ai,[[3, 1], [4, 1], [5, 2], [6, 2], [6, 3], [9, 0], [10, 3], [11, 0], [11, 1], [11, 3], [13, 0], [13, 3], [14, 2], [15, 1], [15, 2], [15, 3], [16, 4]])
py2 = aiPlayer(True,ai,py1,[[3, 1], [4, 1], [5, 2], [6, 2], [6, 3], [9, 0], [10, 3], [11, 0], [11, 1], [11, 3], [13, 0], [13, 3], [14, 2], [15, 1], [15, 2], [15, 3], [16, 4]])
ai = aiPlayer(False,py1,py2,[[4, 0], [4, 1], [4, 2], [5,0], [5, 1], [5, 2], [10, 3], [11, 0], [11, 1],[11,2], [11, 3],[12,2], [13, 0], [13, 3], [14, 2], [15, 1], [15, 2], [15, 3], [16, 4]])

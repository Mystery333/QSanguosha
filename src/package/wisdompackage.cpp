#include "wisdompackage.h"
#include "skill.h"
#include "client.h"
#include "engine.h"
#include "settings.h"
#include "room.h"
#include "maneuvering.h"
#include "general.h"

JuaoCard::JuaoCard(){
    will_throw = false;
    handling_method = MethodNone;
}

bool JuaoCard::targetFilter(const QList<const Player *> &targets, const Player *to_select, const Player *) const{
    return targets.isEmpty() && to_select->getMark("juao") == 0;
}

void JuaoCard::onEffect(const CardEffectStruct &effect) const{
    effect.to->addToPile("hautain", this, false);
    effect.to->addMark("juao");
}

class JuaoViewAsSkill: public ViewAsSkill{
public:
    JuaoViewAsSkill():ViewAsSkill("juao"){

    }

    virtual bool isEnabledAtPlay(const Player *player) const{
        return !player->hasUsed("JuaoCard");
    }

    virtual bool viewFilter(const QList<const Card *> &selected, const Card *to_select) const{
        if(selected.length() >= 2)
            return false;
        return !to_select->isEquipped();
    }

    virtual const Card *viewAs(const QList<const Card *> &cards) const{
        if(cards.length() != 2)
            return NULL;

        JuaoCard *card = new JuaoCard();
        card->addSubcards(cards);
        return card;
    }
};

class Juao: public PhaseChangeSkill{
public:
    Juao():PhaseChangeSkill("juao"){
        view_as_skill = new JuaoViewAsSkill;
    }

    virtual bool triggerable(const ServerPlayer *target) const{
        return target != NULL && target->getMark("juao") > 0;
    }

    virtual bool onPhaseChange(ServerPlayer *player) const{
        if(player->getPhase() == Player::Start){
            Room *room = player->getRoom();
            player->setMark("juao", 0);
            
            LogMessage log;
            log.type = "#JuaoObtain";
            log.from = player;
            log.arg = objectName();
            room->sendLog(log);
            
            DummyCard *dummy = new DummyCard;

            foreach (int card_id, player->getPile("hautain")) {
                dummy->addSubcard(card_id);
            }

            player->obtainCard(dummy, false);
            delete dummy;

            player->skip(Player::Draw);
        }
        return false;
    }
};

class Tanlan: public TriggerSkill{
public:
    Tanlan():TriggerSkill("tanlan"){
        events << Pindian << Damaged;
    }

    virtual bool trigger(TriggerEvent triggerEvent, Room* room, ServerPlayer *xuyou, QVariant &data) const{
        if(triggerEvent == Damaged){
            DamageStruct damage = data.value<DamageStruct>();
            ServerPlayer *from = damage.from;
            Room *room = xuyou->getRoom();
            if(from && from != xuyou && !from->isKongcheng() && !xuyou->isKongcheng() && room->askForSkillInvoke(xuyou, objectName(), data)){
                xuyou->pindian(from, objectName());
            }
            return false;
        }
        else {
            PindianStar pindian = data.value<PindianStar>();
            if(pindian->reason == objectName() && pindian->isSuccess()){
                xuyou->obtainCard(pindian->to_card);
                xuyou->obtainCard(pindian->from_card);
            }
        }
        return false;
    }
};

class Shicai: public TriggerSkill{
public:
    Shicai():TriggerSkill("shicai"){
        events << Pindian;
        frequency = Compulsory;
    }

    virtual bool triggerable(const ServerPlayer *target) const{
        return target != NULL;
    }

    virtual bool trigger(TriggerEvent, Room* room, ServerPlayer *player, QVariant &data) const{
        if (player == NULL) return false;
        ServerPlayer *xuyou = room->findPlayerBySkillName(objectName());
        if(!xuyou) return false;
        PindianStar pindian = data.value<PindianStar>();
        if(pindian->from != xuyou && pindian->to != xuyou)
            return false;
        ServerPlayer *winner = pindian->from_card->getNumber() > pindian->to_card->getNumber() ? pindian->from : pindian->to;
        if(winner == xuyou){
            LogMessage log;
            log.type = "#TriggerSkill";
            log.from = xuyou;
            log.arg = objectName();
            room->sendLog(log);

            xuyou->drawCards(1);
        }
        return false;
    }
};

class Yicai:public TriggerSkill{
public:
    Yicai():TriggerSkill("yicai"){
        events << CardUsed << CardResponded;
    }

    virtual bool trigger(TriggerEvent triggerEvent, Room* room, ServerPlayer *jiangwei, QVariant &data) const{
        CardStar card = NULL;
        if(triggerEvent == CardUsed){
            CardUseStruct use = data.value<CardUseStruct>();
            card = use.card;
        }else if(triggerEvent == CardResponded)
            card = data.value<ResponsedStruct>().m_card;

        if(card && card->isNDTrick())
            if(room->askForSkillInvoke(jiangwei, objectName(), data))
                room->askForUseSlashTo(jiangwei, room->getOtherPlayers(jiangwei), "@askforslash");

        return false;
    }
};

class Beifa: public TriggerSkill{
public:
    Beifa():TriggerSkill("beifa"){
        events << CardsMoveOneTime;
        frequency = Compulsory;
    }
    virtual bool trigger(TriggerEvent, Room* room, ServerPlayer *jiangwei, QVariant &data) const{
        CardsMoveOneTimeStar move = data.value<CardsMoveOneTimeStar>();
        if(move->from == jiangwei && move->from_places.contains(Player::PlaceHand) && jiangwei->isKongcheng())
        {
            QList<ServerPlayer *> players;
            Slash *slash = new Slash(Card::NoSuit, 0);
            slash->setSkillName(objectName());
            foreach(ServerPlayer *player, room->getOtherPlayers(jiangwei))
            {
                if (jiangwei->canSlash(player, slash))
                    players << player;
            }

            ServerPlayer *target = NULL;
            if(!players.isEmpty())
                target = room->askForPlayerChosen(jiangwei, players, objectName());

            if (target == NULL && !jiangwei->isProhibited(jiangwei, slash))
                target = jiangwei;

            CardUseStruct use;
            use.card = slash;
            use.from = jiangwei;
            use.to << target;

            room->useCard(use);
        }
        return false;
    }
};

HouyuanCard::HouyuanCard(){
}

void HouyuanCard::onEffect(const CardEffectStruct &effect) const{
    effect.to->drawCards(2);
}

class Houyuan: public ViewAsSkill{
public:
    Houyuan():ViewAsSkill("houyuan"){
    }

    virtual bool viewFilter(const QList<const Card *> &selected, const Card *to_select) const{
        return !to_select->isEquipped() && selected.length() < 2;
    }

    virtual bool isEnabledAtPlay(const Player *player) const{
        return !player->hasUsed("HouyuanCard");
    }

    virtual const Card *viewAs(const QList<const Card *> &cards) const{
        if(cards.length() != 2)
            return NULL;
        HouyuanCard *card = new HouyuanCard;
        card->addSubcards(cards);
        return card;
    }
};

class Chouliang: public PhaseChangeSkill{
public:
    Chouliang():PhaseChangeSkill("chouliang"){
        frequency = Frequent;
    }

    virtual bool onPhaseChange(ServerPlayer *player) const{
        Room *room = player->getRoom();
        int handcardnum = player->getHandcardNum();
        if(player->getPhase() == Player::Finish && handcardnum < 3
           && room->askForSkillInvoke(player, objectName())){
            int x = 4 - handcardnum;
            QList<int> ids = room->getNCards(x, false);
            CardsMoveStruct move;
            move.card_ids = ids;
            move.to = player;
            move.to_place = Player::PlaceTable;
            move.reason = CardMoveReason(CardMoveReason::S_REASON_TURNOVER, player->objectName(), objectName(), QString());
            room->moveCardsAtomic(move, true);
            room->getThread()->delay(2 * Config.AIDelay);

            QList<int> card_to_throw;
            QList<int> card_to_gotback;
            for (int i = 0; i < x; i++) {
                if (!Sanguosha->getCard(ids[i])->isKindOf("BasicCard"))
                    card_to_throw << ids[i];
                else
                    card_to_gotback << ids[i];
            }
            if (!card_to_gotback.isEmpty()) {
                DummyCard *dummy2 = new DummyCard;
                foreach (int id, card_to_gotback)
                    dummy2->addSubcard(id);

                CardMoveReason reason(CardMoveReason::S_REASON_GOTBACK, player->objectName());
                room->obtainCard(player, dummy2, reason);
                dummy2->deleteLater();
            }
            if (!card_to_throw.isEmpty()) {
                DummyCard *dummy = new DummyCard;
                foreach (int id, card_to_throw)
                    dummy->addSubcard(id);

                CardMoveReason reason(CardMoveReason::S_REASON_NATURAL_ENTER, player->objectName(), objectName(), QString());
                room->throwCard(dummy, reason, NULL);
                dummy->deleteLater();
            }

        }
        return false;
    }
};

BawangCard::BawangCard(){
}

bool BawangCard::targetFilter(const QList<const Player *> &targets, const Player *to_select, const Player *Self) const{
    if(targets.length() >= 2)
        return false;
    return Self->canSlash(to_select, false);
}

void BawangCard::onEffect(const CardEffectStruct &effect) const{
    Room *room = effect.from->getRoom();

    CardUseStruct use;
    Slash *slash = new Slash(Card::NoSuit, 0);
    slash->setSkillName("bawang");
    use.card = slash;
    use.from = effect.from;
    use.to << effect.to;

    room->setEmotion(effect.to, "bad");
    room->setEmotion(effect.from, "good");
    room->useCard(use, false);
}

class BawangViewAsSkill: public ZeroCardViewAsSkill{
public:
    BawangViewAsSkill():ZeroCardViewAsSkill("bawang"){
    }

    virtual const Card *viewAs() const{
        return new BawangCard;
    }

protected:
    virtual bool isEnabledAtPlay(const Player *player) const{
        return false;
    }

    virtual bool isEnabledAtResponse(const Player *player, const QString &pattern) const{
        return  pattern == "@@bawang";
    }
};

class Bawang: public TriggerSkill{
public:
    Bawang():TriggerSkill("bawang"){
        view_as_skill = new BawangViewAsSkill;
        events << SlashMissed;
    }

    virtual bool trigger(TriggerEvent, Room* room, ServerPlayer *sunce, QVariant &data) const{
        SlashEffectStruct effect = data.value<SlashEffectStruct>();
        if(!effect.to->isNude() && !sunce->isKongcheng() && !effect.to->isKongcheng()){
            Room *room = sunce->getRoom();
            if(room->askForSkillInvoke(sunce, objectName(), data)){
                bool success = sunce->pindian(effect.to, objectName(), NULL);
                if(success){
                    if(sunce->hasFlag("drank"))
                        room->setPlayerFlag(sunce, "-drank");
                    room->askForUseCard(sunce, "@@bawang", "@bawang");
                }
            }
        }
        return false;
    }
};

WeidaiCard::WeidaiCard(){
    target_fixed = true;
}

const Card *WeidaiCard::validate(const CardUseStruct *card_use) const {
    ServerPlayer *sunce = card_use->from;
    Room *room = sunce->getRoom();
    foreach (ServerPlayer *liege, room->getLieges("wu", sunce)) {
        QVariant tohelp = QVariant::fromValue((PlayerStar)sunce);
        QString prompt = QString("@weidai-analeptic:%1").arg(sunce->objectName());
        const Card *card = room->askForCard(liege, ".|spade|2~9|hand", prompt, tohelp, Card::MethodDiscard, sunce);
        if(card){
            Analeptic *ana = new Analeptic(card->getSuit(), card->getNumber());
            ana->setSkillName("weidai");
            ana->addSubcard(card);
            return ana;
        }
    }
    return NULL;
}

const Card *WeidaiCard::validateInResponse(ServerPlayer *user, bool &continuable) const {
    continuable = true;
    Room *room = user->getRoom();
    foreach (ServerPlayer *liege, room->getLieges("wu", user)) {
        QVariant tohelp = QVariant::fromValue((PlayerStar)user);
        QString prompt = QString("@weidai-analeptic:%1").arg(user->objectName());
        const Card *card = room->askForCard(liege, ".|spade|2~9|hand", prompt, tohelp, Card::MethodDiscard, user);
        if(card){
            Analeptic *ana = new Analeptic(card->getSuit(), card->getNumber());
            ana->setSkillName("weidai");
            ana->addSubcard(card);
            return ana;
        }
    }
    return NULL;
}

class Weidai:public ZeroCardViewAsSkill{
public:
    Weidai():ZeroCardViewAsSkill("weidai$"){
    }

    virtual bool isEnabledAtPlay(const Player *player) const {
        return player->hasLordSkill("weidai")
                && Analeptic::IsAvailable(player)
                && !player->hasFlag("drank");
    }

    virtual bool isEnabledAtResponse(const Player *player, const QString &pattern) const{
        return pattern == "peach+analeptic";
    }

    virtual const Card *viewAs() const{
        return new WeidaiCard;
    }
};

class Longluo: public TriggerSkill {
public:
    Longluo(): TriggerSkill("longluo") {
        events << CardsMoveOneTime << EventPhaseStart;
    }

    virtual bool trigger(TriggerEvent triggerEvent, Room* room, ServerPlayer *player, QVariant &data) const{
        if (triggerEvent == EventPhaseStart) {
            if (player->getPhase() == Player::Finish) {
                int drawnum = player->getMark(objectName());
                room->setPlayerMark(player, objectName(), 0);
                if (drawnum > 0 && player->askForSkillInvoke(objectName(), data)) {
                    ServerPlayer *target = room->askForPlayerChosen(player, room->getOtherPlayers(player), objectName());
                    target->drawCards(drawnum);
                }
            }
        } else if (player->getPhase() == Player::Discard) {
            CardsMoveOneTimeStar move = data.value<CardsMoveOneTimeStar>();
            if (move->from == player && move->to_place == Player::DiscardPile
                && (move->reason.m_reason & CardMoveReason::S_MASK_BASIC_REASON) == CardMoveReason::S_REASON_DISCARD) {
                room->setPlayerMark(player, objectName(), move->card_ids.length());
            }
        }
        return false;
    }
};

FuzuoCard::FuzuoCard() {
}

bool FuzuoCard::targetFilter(const QList<const Player *> &targets, const Player *to_select, const Player *) const{
    return targets.isEmpty() && to_select->hasFlag("fuzuo_target");
}

void FuzuoCard::onEffect(const CardEffectStruct &effect) const{
    effect.to->getRoom()->setPlayerMark(effect.to, "fuzuo", this->getNumber());
}

class FuzuoViewAsSkill: public OneCardViewAsSkill {
public:
    FuzuoViewAsSkill(): OneCardViewAsSkill("fuzuo") {
    }

    virtual bool isEnabledAtPlay(const Player *player) const{
        return false;
    }

    virtual bool isEnabledAtResponse(const Player *player, const QString &pattern) const{
        return pattern == "@@fuzuo";
    }

    virtual bool viewFilter(const Card* to_select) const{
        return !to_select->isEquipped() && to_select->getNumber() < 8;
    }

    virtual const Card *viewAs(const Card *originalCard) const{
        FuzuoCard *card = new FuzuoCard;
        card->addSubcard(originalCard->getId());
        return card;
    }
};

class Fuzuo: public TriggerSkill {
public:
    Fuzuo(): TriggerSkill("fuzuo") { // @todo_P: adjust AI
        events << PindianVerifying;
        view_as_skill = new FuzuoViewAsSkill;
    }

    virtual bool triggerable(const ServerPlayer *target) const{
        return target != NULL;
    }

    virtual bool trigger(TriggerEvent, Room* room, ServerPlayer *, QVariant &data) const{
        ServerPlayer *zhangzhao = room->findPlayerBySkillName(objectName());
        if (!zhangzhao) return false;
        PindianStar pindian = data.value<PindianStar>();
        room->setPlayerFlag(pindian->from, "fuzuo_target");
        room->setPlayerFlag(pindian->to, "fuzuo_target");
        if (room->askForUseCard(zhangzhao, "@@fuzuo", "@fuzuo-pindian", -1, Card::MethodDiscard)) {
            bool isFrom = (pindian->from->getMark(objectName()) > 0);

            LogMessage log;
            log.type = "$Fuzuo";

            if (isFrom) {
                int to_add = pindian->from->getMark(objectName()) / 2;
                room->setPlayerMark(pindian->from, objectName(), 0);
                pindian->from_number += to_add;

                log.from = pindian->from;
                log.arg = QString::number(pindian->from_number);
            } else {
                int to_add = pindian->to->getMark(objectName()) / 2;
                room->setPlayerMark(pindian->to, objectName(), 0);
                pindian->to_number += to_add;

                log.from = pindian->to;
                log.arg = QString::number(pindian->to_number);
            }

            room->sendLog(log);
        }

        room->setPlayerFlag(pindian->from, "-fuzuo_target");
        room->setPlayerFlag(pindian->to, "-fuzuo_target");

        return false;
    }
};

class Jincui: public TriggerSkill{
public:
    Jincui():TriggerSkill("jincui"){
        events << Death;
    }

    virtual bool triggerable(const ServerPlayer *target) const{
        return target != NULL && target->hasSkill(objectName());
    }

    virtual bool trigger(TriggerEvent, Room *room, ServerPlayer *player, QVariant &data) const{
        DeathStruct death = data.value<DeathStruct>();
        if (death.who != player)
            return false;
        QList<ServerPlayer *> targets = room->getAlivePlayers();
        if (targets.isEmpty() || !player->askForSkillInvoke(objectName()))
            return false;

        ServerPlayer *target = room->askForPlayerChosen(player, targets, objectName());
        QVariant t_data = QVariant::fromValue((PlayerStar)target);
        if (room->askForChoice(player, objectName(), "draw+throw", t_data) == "draw")
            target->drawCards(3);
        else
            room->askForDiscard(target, objectName(), 3, 3, false, true);
        return false;
    }
};

class Badao: public TriggerSkill{
public:
    Badao():TriggerSkill("badao"){
        events << TargetConfirmed;
    }

    virtual bool trigger(TriggerEvent, Room* room, ServerPlayer *hua, QVariant &data) const{
        CardUseStruct use = data.value<CardUseStruct>();
        if(use.card->isKindOf("Slash") && use.card->isBlack() && use.to.contains(hua)){
            room->askForUseCard(hua, "slash", "@askforslash"); // @todo_P: adjust AI
        }
        return false;
    }
};

class Wenjiu: public TriggerSkill{
public:
    Wenjiu():TriggerSkill("wenjiu"){
        events << ConfirmDamage << SlashProceed;
        frequency = Compulsory;
    }
    virtual bool triggerable(const ServerPlayer *target) const{
        return target != NULL;
    }

    virtual int getPriority() const{
        return 3;
    }

    virtual bool trigger(TriggerEvent triggerEvent, Room* room, ServerPlayer *player, QVariant &data) const{
        ServerPlayer *hua = room->findPlayerBySkillName(objectName());
        if (!hua)
            return false;
        if(triggerEvent == SlashProceed){
            SlashEffectStruct effect = data.value<SlashEffectStruct>();
            if(effect.to == hua && effect.slash->isRed()){

                LogMessage log;
                log.type = "#Wenjiu1";
                log.from = effect.from;
                log.to << effect.to;
                room->sendLog(log);

                room->slashResult(effect, NULL);
                return true;
            }
        }
        else if(triggerEvent == ConfirmDamage){
            DamageStruct damage = data.value<DamageStruct>();
            const Card *reason = damage.card;
            if(!reason || damage.from != hua)
                return false;

            if(reason->isKindOf("Slash") && reason->isBlack()){
                LogMessage log;
                log.type = "#Wenjiu2";
                log.from = hua;
                log.to << damage.to;
                log.arg = QString::number(damage.damage);
                log.arg2 = QString::number(++ damage.damage);
                room->sendLog(log);
                data = QVariant::fromValue(damage);
            }
        }

        return false;
    }
};

class Shipo: public TriggerSkill{
public:
    Shipo():TriggerSkill("shipo"){
        events << EventPhaseStart;
    }

    virtual bool triggerable(const ServerPlayer *target) const{
        return target != NULL;
    }

    virtual bool trigger(TriggerEvent, Room* room, ServerPlayer *player, QVariant &data) const{
        if(player->getPhase() != Player::Judge || player->getJudgingArea().length() == 0)
            return false;
        QList<ServerPlayer *> tians = room->findPlayersBySkillName(objectName());
        foreach(ServerPlayer *tianfeng, tians){
            if(tianfeng->getCardCount(true)>=2
               && room->askForSkillInvoke(tianfeng, objectName(), QVariant::fromValue(player))
                && room->askForDiscard(tianfeng, objectName(), 2, 2, false, true)){

                    DummyCard *dummy = new DummyCard;
                    dummy->addSubcards(player->getJudgingArea());
                    tianfeng->obtainCard(dummy);
                    delete dummy;
                    break;
            }
        }
        return false;
    }
};

class Gushou:public TriggerSkill{
public:
    Gushou():TriggerSkill("gushou"){
        frequency = Frequent;
        events << CardUsed << CardResponded;
    }

    virtual bool trigger(TriggerEvent triggerEvent, Room* room, ServerPlayer *tianfeng, QVariant &data) const{
        if(room->getCurrent() == tianfeng)
            return false;
        CardStar card = NULL;
        if(triggerEvent == CardUsed){
            CardUseStruct use = data.value<CardUseStruct>();
            card = use.card;
        }else if(triggerEvent == CardResponded)
            card = data.value<ResponsedStruct>().m_card;

        if(card && card->isKindOf("BasicCard")){
            if(room->askForSkillInvoke(tianfeng, objectName(), data)){
                room->broadcastSkillInvoke(objectName());
                tianfeng->drawCards(1);
            }
        }

        return false;
    }
};

class Yuwen: public TriggerSkill{
public:
    Yuwen():TriggerSkill("yuwen"){
        events << GameOverJudge;
        frequency = Compulsory;
    }

    virtual int getPriority() const{
        return 4;
    }

    virtual bool triggerable(const ServerPlayer *target) const{
        return target != NULL && target->hasSkill(objectName());
    }

    virtual bool trigger(TriggerEvent, Room* room, ServerPlayer *tianfeng, QVariant &data) const{
        DamageStar damage = data.value<DamageStar>();

        if(damage){
            if(damage->from == tianfeng)
                return false;
        }else{
            damage = new DamageStruct;
            damage->to = tianfeng;
            data = QVariant::fromValue(damage);
        }

        damage->from = tianfeng;

        LogMessage log;
        log.type = "#TriggerSkill";
        log.from = tianfeng;
        log.arg = objectName();
        room->sendLog(log);

        return false;
    }
};

ShouyeCard::ShouyeCard(){
}

bool ShouyeCard::targetFilter(const QList<const Player *> &targets, const Player *to_select, const Player *Self) const{
    if(targets.length() >= 2)
        return false;

    if(to_select == Self)
        return false;
    return true;
}

void ShouyeCard::onEffect(const CardEffectStruct &effect) const{
    effect.to->drawCards(1);
    if(effect.from->getMark("jiehuo") == 0)
        effect.from->gainMark("@shouye");
}

class Shouye: public OneCardViewAsSkill{
public:
    Shouye():OneCardViewAsSkill("shouye"){

    }

    virtual bool isEnabledAtPlay(const Player *player) const{
        if(player->getMark("jiehuo") > 0)
            return !player->hasUsed("ShouyeCard");
        else
            return true;
    }

    virtual bool viewFilter(const Card* to_select) const{
        return !to_select->isEquipped() && to_select->isRed();
    }

    virtual const Card *viewAs(const Card *originalCard) const{
        ShouyeCard *shouye_card = new ShouyeCard;
        shouye_card->addSubcard(originalCard->getId());

        return shouye_card;
    }
};

class Jiehuo: public TriggerSkill{
public:
    Jiehuo():TriggerSkill("jiehuo"){
        events << CardFinished;
        frequency = Wake;
    }

    virtual bool triggerable(const ServerPlayer *target) const{
        return target != NULL && target->getMark("jiehuo") == 0
                && target->getMark("@shouye") >= 7;
    }

    virtual bool trigger(TriggerEvent, Room* room, ServerPlayer *player, QVariant &data) const{
        LogMessage log;
        log.type = "#JiehuoWake";
        log.from = player;
        log.arg = objectName();
        log.arg2 = "shouye";
        room->sendLog(log);

        room->setPlayerMark(player, "jiehuo", 1);
        player->loseAllMarks("@shouye");
        room->acquireSkill(player, "shien");

        room->loseMaxHp(player);
        return false;
    }
};

class Shien:public TriggerSkill{
public:
    Shien():TriggerSkill("shien"){
        events << CardUsed << CardResponded;
    }

    virtual bool triggerable(const ServerPlayer *target) const{
        return target != NULL && !target->hasSkill(objectName());
    }

    virtual bool trigger(TriggerEvent triggerEvent, Room* room, ServerPlayer *player, QVariant &data) const{
        if (player == NULL) return false;
        if (player->getMark("forbid_shien") > 0 || player->hasFlag("forbid_shien"))
            return false;
        CardStar card = NULL;
        if(triggerEvent == CardUsed){
            CardUseStruct use = data.value<CardUseStruct>();
            card = use.card;
        }else if(triggerEvent == CardResponded)
            card = data.value<ResponsedStruct>().m_card;

        if(card && card->isNDTrick()){
            ServerPlayer *shuijing = room->findPlayerBySkillName(objectName());
            if (!shuijing)
                return false;

            if(room->askForSkillInvoke(player, objectName(), QVariant::fromValue((PlayerStar)shuijing)))
                shuijing->drawCards(1);
            else{
                QString choice = room->askForChoice(player, "forbid_shien", "yes+no+maybe");
                if(choice == "yes")
                    room->setPlayerMark(player, "forbid_shien", 1);
                else if (choice == "maybe")
                    room->setPlayerFlag(player, "forbid_shien");
            }
        }

        return false;
    }
};

WisdomPackage::WisdomPackage()
    :Package("wisdom")
{

    General *wisxuyou,
            *wisjiangwei, *wisjiangwan,
            *wissunce, *wiszhangzhao,
            *wishuaxiong, *wistianfeng, *wisshuijing;

        wisxuyou = new General(this, "wisxuyou", "wei", 3);
        wisxuyou->addSkill(new Juao);
        wisxuyou->addSkill(new Tanlan);
        wisxuyou->addSkill(new Shicai);

        wisjiangwei = new General(this, "wisjiangwei", "shu");
        wisjiangwei->addSkill(new Yicai);
        wisjiangwei->addSkill(new Beifa);

        wisjiangwan = new General(this, "wisjiangwan", "shu", 3);
        wisjiangwan->addSkill(new Houyuan);
        wisjiangwan->addSkill(new Chouliang);

        wissunce = new General(this, "wissunce$", "wu");
        wissunce->addSkill(new Bawang);
        wissunce->addSkill(new Weidai);

        wiszhangzhao = new General(this, "wiszhangzhao", "wu", 3);
        wiszhangzhao->addSkill(new Longluo);
        wiszhangzhao->addSkill(new Fuzuo);
        wiszhangzhao->addSkill(new Jincui);

        wishuaxiong = new General(this, "wishuaxiong", "qun");
        wishuaxiong->addSkill(new Badao);
        wishuaxiong->addSkill(new Wenjiu);

        wistianfeng = new General(this, "wistianfeng", "qun", 3);
        wistianfeng->addSkill(new Shipo);
        wistianfeng->addSkill(new Gushou);
        wistianfeng->addSkill(new Yuwen);

        wisshuijing = new General(this, "wisshuijing", "qun");
        wisshuijing->addSkill(new Shouye);
        wisshuijing->addSkill(new Jiehuo);

        skills << new Shien;

        addMetaObject<JuaoCard>();
        addMetaObject<BawangCard>();
        addMetaObject<FuzuoCard>();
        addMetaObject<WeidaiCard>();
        addMetaObject<HouyuanCard>();
        addMetaObject<ShouyeCard>();
}

ADD_PACKAGE(Wisdom)

#include "../Player.h"
#include "PlayerbotAI.h"
#include "PlayerbotMgr.h"
#include "WorldPacket.h"
#include "../Chat.h"
#include "../ObjectMgr.h"
#include "../GuildMgr.h"
#include "../GossipDef.h"
#include "../Chat.h"
#include "../Language.h"
#include "../Guild.h"

class LoginQueryHolder;
class CharacterHandler;

PlayerbotMgr::PlayerbotMgr(Player* const master) : m_master(master)
{
    // load config variables
    m_confMaxNumBots = sWorld.getConfig(CONFIG_UINT32_PLAYERBOT_MAXBOTS);
    m_confDebugWhisper = sWorld.getConfig(CONFIG_BOOL_PLAYERBOT_DEBUGWHISPER);
    m_confFollowDistance[0] = sWorld.getConfig(CONFIG_FLOAT_PLAYERBOT_MINDISTANCE);
    m_confFollowDistance[1] = sWorld.getConfig(CONFIG_FLOAT_PLAYERBOT_MAXDISTANCE);
}

PlayerbotMgr::~PlayerbotMgr()
{
    LogoutAllBots();
}

void PlayerbotMgr::UpdateAI(const uint32 p_time) {}

void PlayerbotMgr::HandleMasterIncomingPacket(const WorldPacket& packet)
{
    switch (packet.GetOpcode())
    {
        // if master is logging out, log out all bots
        case CMSG_LOGOUT_REQUEST:
        {
            LogoutAllBots();
            return;
        }

        // If master inspects one of his bots, give the master useful info in chat window
        // such as inventory that can be equipped
        case CMSG_INSPECT:
        {
            WorldPacket p(packet);
            p.rpos(0); // reset reader
            ObjectGuid guid;
            p >> guid;
            Player* const bot = GetPlayerBot(guid);
            //if (bot) bot->GetPlayerbotAI()->SendNotEquipList(*bot);
            return;
        }

        // handle emotes from the master
        //case CMSG_EMOTE:
        case CMSG_TEXT_EMOTE:
        {
            WorldPacket p(packet);
            p.rpos(0); // reset reader
            uint32 emoteNum;
            p >> emoteNum;

            /* std::ostringstream out;
               out << "emote is: " << emoteNum;
               ChatHandler ch(m_master);
               ch.SendSysMessage(out.str().c_str()); */

            switch (emoteNum)
            {
                case TEXTEMOTE_BOW:
                {
                    // Buff anyone who bows before me. Useful for players not in bot's group
                    // How do I get correct target???
                    //Player* const pPlayer = GetPlayerBot(m_master->GetSelection());
                    //if (pPlayer->GetPlayerbotAI()->GetClassAI())
                    //    pPlayer->GetPlayerbotAI()->GetClassAI()->BuffPlayer(pPlayer);
                    return;
                }
                /*
                   case TEXTEMOTE_BONK:
                   {
                    Player* const pPlayer = GetPlayerBot(m_master->GetSelection());
                    if (!pPlayer || !pPlayer->GetPlayerbotAI())
                        return;
                    PlayerbotAI* const pBot = pPlayer->GetPlayerbotAI();

                    ChatHandler ch(m_master);
                    {
                        std::ostringstream out;
                        out << "time(0): " << time(0)
                            << " m_ignoreAIUpdatesUntilTime: " << pBot->m_ignoreAIUpdatesUntilTime;
                        ch.SendSysMessage(out.str().c_str());
                    }
                    {
                        std::ostringstream out;
                        out << "m_TimeDoneEating: " << pBot->m_TimeDoneEating
                            << " m_TimeDoneDrinking: " << pBot->m_TimeDoneDrinking;
                        ch.SendSysMessage(out.str().c_str());
                    }
                    {
                        std::ostringstream out;
                        out << "m_CurrentlyCastingSpellId: " << pBot->m_CurrentlyCastingSpellId;
                        ch.SendSysMessage(out.str().c_str());
                    }
                    {
                        std::ostringstream out;
                        out << "IsBeingTeleported() " << pBot->GetPlayer()->IsBeingTeleported();
                        ch.SendSysMessage(out.str().c_str());
                    }
                    {
                        std::ostringstream out;
                        bool tradeActive = (pBot->GetPlayer()->GetTrader()) ? true : false;
                        out << "tradeActive: " << tradeActive;
                        ch.SendSysMessage(out.str().c_str());
                    }
                    {
                        std::ostringstream out;
                        out << "IsCharmed() " << pBot->getPlayer()->isCharmed();
                        ch.SendSysMessage(out.str().c_str());
                    }
                    return;
                   }
                 */

                case TEXTEMOTE_EAT:
                case TEXTEMOTE_DRINK:
                {
                    for (PlayerBotMap::const_iterator it = GetPlayerBotsBegin(); it != GetPlayerBotsEnd(); ++it)
                    {
                        Player* const bot = it->second;
                        bot->GetPlayerbotAI()->Feast();
                    }
                    return;
                }

                // emote to attack selected target
                case TEXTEMOTE_POINT:
                {
                    ObjectGuid attackOnGuid = m_master->GetSelectionGuid();
                    if (attackOnGuid.IsEmpty())
                        return;

                    Unit* thingToAttack = ObjectAccessor::GetUnit(*m_master, attackOnGuid);
                    if (!thingToAttack) return;

                    Player *bot = 0;
                    for (PlayerBotMap::iterator itr = m_playerBots.begin(); itr != m_playerBots.end(); ++itr)
                    {
                        bot = itr->second;
                        if (!bot->IsFriendlyTo(thingToAttack) && bot->IsWithinLOSInMap(thingToAttack))
                            bot->GetPlayerbotAI()->GetCombatTarget(thingToAttack);
                    }
                    return;
                }

                // emote to stay
                case TEXTEMOTE_STAND:
                {
                    Player* const bot = GetPlayerBot(m_master->GetSelectionGuid());
                    if (bot)
                        bot->GetPlayerbotAI()->SetMovementOrder(PlayerbotAI::MOVEMENT_STAY);
                    else
                        for (PlayerBotMap::const_iterator it = GetPlayerBotsBegin(); it != GetPlayerBotsEnd(); ++it)
                        {
                            Player* const bot = it->second;
                            bot->GetPlayerbotAI()->SetMovementOrder(PlayerbotAI::MOVEMENT_STAY);
                        }
                    return;
                }

                // 324 is the followme emote (not defined in enum)
                // if master has bot selected then only bot follows, else all bots follow
                case 324:
                case TEXTEMOTE_WAVE:
                {
                    Player* const bot = GetPlayerBot(m_master->GetSelectionGuid());
                    if (bot)
                        bot->GetPlayerbotAI()->SetMovementOrder(PlayerbotAI::MOVEMENT_FOLLOW, m_master);
                    else
                        for (PlayerBotMap::const_iterator it = GetPlayerBotsBegin(); it != GetPlayerBotsEnd(); ++it)
                        {
                            Player* const bot = it->second;
                            bot->GetPlayerbotAI()->SetMovementOrder(PlayerbotAI::MOVEMENT_FOLLOW, m_master);
                        }
                    return;
                }
            }
            return;
        } /* EMOTE ends here */

        case CMSG_GAMEOBJ_USE: // not sure if we still need this one
        case CMSG_GAMEOBJ_REPORT_USE:
        {
            WorldPacket p(packet);
            p.rpos(0);     // reset reader
            ObjectGuid objGUID;
            p >> objGUID;

            GameObject *obj = (m_master && m_master->GetMap()) ? m_master->GetMap()->GetGameObject(objGUID) : NULL;
            if (!obj)
                return;

            for (PlayerBotMap::const_iterator it = GetPlayerBotsBegin(); it != GetPlayerBotsEnd(); ++it)
            {
                Player* const bot = it->second;

                if (obj->GetGoType() == GAMEOBJECT_TYPE_QUESTGIVER)
                    bot->GetPlayerbotAI()->TurnInQuests(obj);
                // add other go types here, i.e.:
                // GAMEOBJECT_TYPE_CHEST - loot quest items of chest
            }
        }
        break;

        // if master talks to an NPC
        case CMSG_GOSSIP_HELLO:
        case CMSG_QUESTGIVER_HELLO:
        {
            WorldPacket p(packet);
            p.rpos(0);    // reset reader
            ObjectGuid npcGUID;
            p >> npcGUID;

            WorldObject* pNpc = (m_master && m_master->GetMap()) ? m_master->GetMap()->GetWorldObject(npcGUID) : NULL;
            if (!pNpc)
                return;

            // for all master's bots
            for (PlayerBotMap::const_iterator it = GetPlayerBotsBegin(); it != GetPlayerBotsEnd(); ++it)
            {
                Player* const bot = it->second;
                bot->GetPlayerbotAI()->TurnInQuests(pNpc);
            }

            return;
        }

        // if master accepts a quest, bots should also try to accept quest
        case CMSG_QUESTGIVER_ACCEPT_QUEST:
        {
            WorldPacket p(packet);
            p.rpos(0);    // reset reader
            ObjectGuid guid;
            uint32 quest;
            p >> guid >> quest;
            Quest const* qInfo = sObjectMgr.GetQuestTemplate(quest);
            if (qInfo)
                for (PlayerBotMap::const_iterator it = GetPlayerBotsBegin(); it != GetPlayerBotsEnd(); ++it)
                {
                    Player* const bot = it->second;

                    if (bot->GetQuestStatus(quest) == QUEST_STATUS_COMPLETE)
                        bot->GetPlayerbotAI()->TellMaster("　我已经完成那个任务啦~!　");
                    else if (!bot->CanTakeQuest(qInfo, false))
                    {
                        if (!bot->SatisfyQuestStatus(qInfo, false))
                            bot->GetPlayerbotAI()->TellMaster("　我已经接受了该任务　");
                        else
                            bot->GetPlayerbotAI()->TellMaster("　我不能接受该任务　");
                    }
                    else if (!bot->SatisfyQuestLog(false))
                        bot->GetPlayerbotAI()->TellMaster("　任务记录满了．．　");
                    else if (!bot->CanAddQuest(qInfo, false))
                        bot->GetPlayerbotAI()->TellMaster("　我包包满了．．装不下任务物品，接不了任务！　");

                    else
                    {
                        p.rpos(0);         // reset reader
                        bot->GetSession()->HandleQuestgiverAcceptQuestOpcode(p);
                        bot->GetPlayerbotAI()->TellMaster("　接受任务　");
                    }
                }
            return;
        }
        case CMSG_LOOT_ROLL:
        {

            WorldPacket p(packet);    //WorldPacket packet for CMSG_LOOT_ROLL, (8+4+1)
            ObjectGuid Guid;
            uint32 NumberOfPlayers;
            uint8 rollType;
            p.rpos(0);    //reset packet pointer
            p >> Guid;    //guid of the item rolled
            p >> NumberOfPlayers;    //number of players invited to roll
            p >> rollType;    //need,greed or pass on roll


            for (PlayerBotMap::const_iterator it = GetPlayerBotsBegin(); it != GetPlayerBotsEnd(); ++it)
            {

                uint32 choice = urand(0, 3);    //returns 0,1,2 or 3

                Player* const bot = it->second;
                if (!bot)
                    return;

                Group* group = bot->GetGroup();
                if (!group)
                    return;

                group->CountRollVote(bot, Guid, NumberOfPlayers, RollVote(choice));

                switch (choice)
                {
                    case ROLL_NEED:
                        bot->GetAchievementMgr().UpdateAchievementCriteria(ACHIEVEMENT_CRITERIA_TYPE_ROLL_NEED, 1);
                        break;
                    case ROLL_GREED:
                        bot->GetAchievementMgr().UpdateAchievementCriteria(ACHIEVEMENT_CRITERIA_TYPE_ROLL_GREED, 1);
                        break;
                }
            }
            return;
        }
        case CMSG_REPAIR_ITEM:
        {

            WorldPacket p(packet);    // WorldPacket packet for CMSG_REPAIR_ITEM, (8+8+1)

            sLog.outDebug("PlayerbotMgr: CMSG_REPAIR_ITEM");

            ObjectGuid npcGUID;
            uint64 itemGUID;
            uint8 guildBank;

            p.rpos(0);    //reset packet pointer
            p >> npcGUID;
            p >> itemGUID;     // Not used for bot but necessary opcode data retrieval
            p >> guildBank;    // Flagged if guild repair selected

            for (PlayerBotMap::const_iterator it = GetPlayerBotsBegin(); it != GetPlayerBotsEnd(); ++it)
            {

                Player* const bot = it->second;
                if (!bot)
                    return;

                Group* group = bot->GetGroup();      // check if bot is a member of group
                if (!group)
                    return;

                Creature *unit = bot->GetNPCIfCanInteractWith(npcGUID, UNIT_NPC_FLAG_REPAIR);
                if (!unit)     // Check if NPC can repair bot or not
                {
                    sLog.outDebug("PlayerbotMgr: HandleRepairItemOpcode - Unit (GUID: %s) not found or you can't interact with him.", npcGUID.GetString().c_str());
                    return;
                }

                // remove fake death
                if (bot->hasUnitState(UNIT_STAT_DIED))
                    bot->RemoveSpellsCausingAura(SPELL_AURA_FEIGN_DEATH);

                // reputation discount
                float discountMod = bot->GetReputationPriceDiscount(unit);

                uint32 TotalCost = 0;
                if (itemGUID)     // Handle redundant feature (repair individual item) for bot
                {
                    sLog.outDebug("ITEM: Repair single item is not applicable for %s", bot->GetName());
                    continue;
                }
                else      // Handle feature (repair all items) for bot
                {
                    sLog.outDebug("ITEM: Repair all items, npcGUID = %s", npcGUID.GetString().c_str());

                    TotalCost = bot->DurabilityRepairAll(true, discountMod, guildBank > 0 ? true : false);
                }
                if (guildBank)     // Handle guild repair
                {
                    uint32 GuildId = bot->GetGuildId();
                    if (!GuildId)
                        return;
                    Guild *pGuild = sGuildMgr.GetGuildById(GuildId);
                    if (!pGuild)
                        return;
                    pGuild->LogBankEvent(GUILD_BANK_LOG_REPAIR_MONEY, 0, bot->GetGUIDLow(), TotalCost);
                    pGuild->SendMoneyInfo(bot->GetSession(), bot->GetGUIDLow());
                }

            }
            return;
        }
        case CMSG_SPIRIT_HEALER_ACTIVATE:
        {
            // sLog.outDebug("SpiritHealer is resurrecting the Player %s",m_master->GetName());
            for (PlayerBotMap::iterator itr = m_playerBots.begin(); itr != m_playerBots.end(); ++itr)
            {
                Player* const bot = itr->second;
                Group *grp = bot->GetGroup();
                if (grp)
                    grp->RemoveMember(bot->GetObjectGuid(), 1);
            }
            return;
        }

            /*
               case CMSG_NAME_QUERY:
               case MSG_MOVE_START_FORWARD:
               case MSG_MOVE_STOP:
               case MSG_MOVE_SET_FACING:
               case MSG_MOVE_START_STRAFE_LEFT:
               case MSG_MOVE_START_STRAFE_RIGHT:
               case MSG_MOVE_STOP_STRAFE:
               case MSG_MOVE_START_BACKWARD:
               case MSG_MOVE_HEARTBEAT:
               case CMSG_STANDSTATECHANGE:
               case CMSG_QUERY_TIME:
               case CMSG_CREATURE_QUERY:
               case CMSG_GAMEOBJECT_QUERY:
               case MSG_MOVE_JUMP:
               case MSG_MOVE_FALL_LAND:
                return;

               default:
               {
                const char* oc = LookupOpcodeName(packet.GetOpcode());
                // ChatHandler ch(m_master);
                // ch.SendSysMessage(oc);

                std::ostringstream out;
                out << "masterin: " << oc;
                sLog.outError(out.str().c_str());
               }
             */
    }
}
void PlayerbotMgr::HandleMasterOutgoingPacket(const WorldPacket& packet)
{
    /*
       switch (packet.GetOpcode())
       {
        // maybe our bots should only start looting after the master loots?
        //case SMSG_LOOT_RELEASE_RESPONSE: {}
        case SMSG_NAME_QUERY_RESPONSE:
        case SMSG_MONSTER_MOVE:
        case SMSG_COMPRESSED_UPDATE_OBJECT:
        case SMSG_DESTROY_OBJECT:
        case SMSG_UPDATE_OBJECT:
        case SMSG_STANDSTATE_UPDATE:
        case MSG_MOVE_HEARTBEAT:
        case SMSG_QUERY_TIME_RESPONSE:
        case SMSG_AURA_UPDATE_ALL:
        case SMSG_CREATURE_QUERY_RESPONSE:
        case SMSG_GAMEOBJECT_QUERY_RESPONSE:
            return;
        default:
        {
            const char* oc = LookupOpcodeName(packet.GetOpcode());

            std::ostringstream out;
            out << "masterout: " << oc;
            sLog.outError(out.str().c_str());
        }
       }
     */
}

void PlayerbotMgr::LogoutAllBots()
{
    while (true)
    {
        PlayerBotMap::const_iterator itr = GetPlayerBotsBegin();
        if (itr == GetPlayerBotsEnd()) break;
        Player* bot = itr->second;
        LogoutPlayerBot(bot->GetObjectGuid());
    }
}



void PlayerbotMgr::Stay()
{
    for (PlayerBotMap::const_iterator itr = GetPlayerBotsBegin(); itr != GetPlayerBotsEnd(); ++itr)
    {
        Player* bot = itr->second;
        bot->GetMotionMaster()->Clear();
    }
}


// Playerbot mod: logs out a Playerbot.
void PlayerbotMgr::LogoutPlayerBot(ObjectGuid guid)
{
    Player* bot = GetPlayerBot(guid);
    if (bot)
    {
        WorldSession * botWorldSessionPtr = bot->GetSession();
        m_playerBots.erase(guid);    // deletes bot player ptr inside this WorldSession PlayerBotMap
        botWorldSessionPtr->LogoutPlayer(true); // this will delete the bot Player object and PlayerbotAI object
        delete botWorldSessionPtr;  // finally delete the bot's WorldSession
    }
}

// Playerbot mod: Gets a player bot Player object for this WorldSession master
Player* PlayerbotMgr::GetPlayerBot(ObjectGuid playerGuid) const
{
    PlayerBotMap::const_iterator it = m_playerBots.find(playerGuid);
    return (it == m_playerBots.end()) ? 0 : it->second;
}

void PlayerbotMgr::OnBotLogin(Player * const bot)
{
    bot->SetMap(sMapMgr.CreateMap(bot->GetMapId(), bot));
    // give the bot some AI, object is owned by the player class
    PlayerbotAI* ai = new PlayerbotAI(this, bot);
    bot->SetPlayerbotAI(ai);

    // tell the world session that they now manage this new bot
    m_playerBots[bot->GetObjectGuid()] = bot;

    // if bot is in a group and master is not in group then
    // have bot leave their group
    if (bot->GetGroup() &&
        (m_master->GetGroup() == NULL ||
         m_master->GetGroup()->IsMember(bot->GetObjectGuid()) == false))
        bot->RemoveFromGroup();

    // sometimes master can lose leadership, pass leadership to master check
    ObjectGuid masterGuid = m_master->GetObjectGuid();
    if (m_master->GetGroup() &&
        !m_master->GetGroup()->IsLeader(masterGuid))
        m_master->GetGroup()->ChangeLeader(masterGuid);
}

void PlayerbotMgr::RemoveAllBotsFromGroup()
{
    for (PlayerBotMap::const_iterator it = GetPlayerBotsBegin(); m_master->GetGroup() && it != GetPlayerBotsEnd(); ++it)
    {
        Player* const bot = it->second;
        if (bot->IsInSameGroupWith(m_master))
            m_master->GetGroup()->RemoveMember(bot->GetObjectGuid(), 0);
    }
}

void Player::skill(std::list<uint32>& m_spellsToLearn)
{
    for (SkillStatusMap::const_iterator itr = mSkillStatus.begin(); itr != mSkillStatus.end(); ++itr)
    {
        if (itr->second.uState == SKILL_DELETED)
            continue;

        uint32 pskill = itr->first;

        m_spellsToLearn.push_back(pskill);
    }
}

void Player::chompAndTrim(std::string& str)
{
    while (str.length() > 0)
    {
        char lc = str[str.length() - 1];
        if (lc == '\r' || lc == '\n' || lc == ' ' || lc == '"' || lc == '\'')
            str = str.substr(0, str.length() - 1);
        else
            break;
        while (str.length() > 0)
        {
            char lc = str[0];
            if (lc == ' ' || lc == '"' || lc == '\'')
                str = str.substr(1, str.length() - 1);
            else
                break;
        }
    }
}

bool Player::getNextQuestId(const std::string& pString, unsigned int& pStartPos, unsigned int& pId)
{
    bool result = false;
    unsigned int i;
    for (i = pStartPos; i < pString.size(); ++i)
    {
        if (pString[i] == ',')
            break;
    }
    if (i > pStartPos)
    {
        std::string idString = pString.substr(pStartPos, i - pStartPos);
        pStartPos = i + 1;
        chompAndTrim(idString);
        pId = atoi(idString.c_str());
        result = true;
    }
    return(result);
}

bool Player::requiredQuests(const char* pQuestIdString)
{
    if (pQuestIdString != NULL)
    {
        unsigned int pos = 0;
        unsigned int id;
        std::string confString(pQuestIdString);
        chompAndTrim(confString);
        while (getNextQuestId(confString, pos, id))
        {
            QuestStatus status = GetQuestStatus(id);
            if (status == QUEST_STATUS_COMPLETE)
                return true;
        }
    }
    return false;
}

bool ChatHandler::HandlePlayerbotCommand(char* args)
{
    if (sWorld.getConfig(CONFIG_BOOL_PLAYERBOT_DISABLE))
    {
        PSendSysMessage(LANG_PLAYERBOT_CMD_DISABLE);
        SetSentErrorMessage(true);
        return false;
    }

    if (m_session->GetPlayer()->GetMap()->IsBattleGroundOrArena() || m_session->GetPlayer()->GetMap()->IsDungeon())
	{
        PSendSysMessage(LANG_PLAYERBOT_CMD_NOTHERE);
		SetSentErrorMessage(true);
		return false;
	}

    if (!m_session)
    {
        PSendSysMessage(LANG_PLAYERBOT_CMD_NOTHERE);
        SetSentErrorMessage(true);
        return false;
    }

    if (!*args)
    {
        PSendSysMessage(LANG_PLAYERBOT_CMD_SYNTAX);
        SetSentErrorMessage(true);
        return false;
    }

    char *cmd = strtok ((char *) args, " ");
    char *charname = strtok (NULL, " ");
    if (!cmd || !charname)
    {
        PSendSysMessage(LANG_PLAYERBOT_CMD_SYNTAX);
        SetSentErrorMessage(true);
        return false;
    }

    std::string cmdStr = cmd;
    std::string charnameStr = charname;

    if (!normalizePlayerName(charnameStr))
        return false;

    ObjectGuid guid = sObjectMgr.GetPlayerGuidByName(charnameStr.c_str());
    if (guid.IsEmpty() || (guid == m_session->GetPlayer()->GetObjectGuid()))
    {
        SendSysMessage(LANG_PLAYER_NOT_FOUND);
        SetSentErrorMessage(true);
        return false;
    }

    uint32 accountId = sObjectMgr.GetPlayerAccountIdByGUID(guid);
    if (accountId != m_session->GetAccountId())
    {
        PSendSysMessage(LANG_PLAYERBOT_CMD_ACCOUNT);
        SetSentErrorMessage(true);
        return false;
    }

    // create the playerbot manager if it doesn't already exist
    PlayerbotMgr* mgr = m_session->GetPlayer()->GetPlayerbotMgr();
    if (!mgr)
    {
        mgr = new PlayerbotMgr(m_session->GetPlayer());
        m_session->GetPlayer()->SetPlayerbotMgr(mgr);
    }

    QueryResult *resultchar = CharacterDatabase.PQuery("SELECT COUNT(*) FROM characters WHERE online = '1' AND account = '%u'", accountId);
    if (resultchar)
    {
        Field *fields = resultchar->Fetch();
        int acctcharcount = fields[0].GetUInt32();
        int maxnum = sWorld.getConfig(CONFIG_UINT32_PLAYERBOT_MAXBOTS);
        if (!(m_session->GetSecurity() > SEC_PLAYER))
            if (acctcharcount > maxnum && (cmdStr == "add" || cmdStr == "login"))
            {
                PSendSysMessage(LANG_PLAYERBOT_CMD_NOTMORE, maxnum);
                SetSentErrorMessage(true);
                delete resultchar;
                return false;
            }
        delete resultchar;
    }

    QueryResult *resultlvl = CharacterDatabase.PQuery("SELECT level,name FROM characters WHERE guid = '%u'", guid.GetCounter());
    if (resultlvl)
    {
        Field *fields = resultlvl->Fetch();
        int charlvl = fields[0].GetUInt32();
        int maxlvl = sWorld.getConfig(CONFIG_UINT32_PLAYERBOT_RESTRICTLEVEL);
        if (!(m_session->GetSecurity() > SEC_PLAYER))
            if (charlvl > maxlvl)
            {
                PSendSysMessage(LANG_PLAYERBOT_CMD_LEVEL, fields[1].GetString(), maxlvl);
                SetSentErrorMessage(true);
                delete resultlvl;
                return false;
            }
        delete resultlvl;
    }
    // end of gmconfig patch
    if (cmdStr == "add" || cmdStr == "login")
    {
        if (mgr->GetPlayerBot(guid))
        {
            PSendSysMessage(LANG_PLAYERBOT_CMD_EXIST);
            SetSentErrorMessage(true);
            return false;
        }
        CharacterDatabase.DirectPExecute("UPDATE characters SET online = 1 WHERE guid = '%u'", guid.GetCounter());
        mgr->AddPlayerBot(guid);
        PSendSysMessage(LANG_PLAYERBOT_CMD_SUCCESS);
    }
    else if (cmdStr == "remove" || cmdStr == "logout")
    {
        if (!mgr->GetPlayerBot(guid))
        {
            PSendSysMessage(LANG_PLAYERBOT_CMD_NOTEXT);
            SetSentErrorMessage(true);
            return false;
        }
        CharacterDatabase.DirectPExecute("UPDATE characters SET online = 0 WHERE guid = '%u'", guid.GetCounter());
        mgr->LogoutPlayerBot(guid);
        PSendSysMessage(LANG_PLAYERBOT_CMD_REMOVE);
    }
    else if (cmdStr == "co" || cmdStr == "combatorder")
    {
        Unit *target = NULL;
        char *orderChar = strtok(NULL, " ");
        if (!orderChar)
        {
            PSendSysMessage(LANG_PLAYERBOT_CMD_COMBAT);
            SetSentErrorMessage(true);
            return false;
        }
        std::string orderStr = orderChar;
        if (orderStr == "protect" || orderStr == "assist")
        {
            char *targetChar = strtok(NULL, " ");
            ObjectGuid targetGuid = m_session->GetPlayer()->GetSelectionGuid();
            if (!targetChar && targetGuid.IsEmpty())
            {
                PSendSysMessage(LANG_PLAYERBOT_CMD_ORDER);
                SetSentErrorMessage(true);
                return false;
            }
            if (targetChar)
            {
                std::string targetStr = targetChar;
                targetGuid = sObjectMgr.GetPlayerGuidByName(targetStr.c_str());
            }
            target = ObjectAccessor::GetUnit(*m_session->GetPlayer(), targetGuid);
            if (!target)
            {
                PSendSysMessage(LANG_PLAYERBOT_CMD_ERRTARGET);
                SetSentErrorMessage(true);
                return false;
            }
        }
        if (mgr->GetPlayerBot(guid) == NULL)
        {
            PSendSysMessage(LANG_PLAYERBOT_CMD_ERRORDER);
            SetSentErrorMessage(true);
            return false;
        }
        mgr->GetPlayerBot(guid)->GetPlayerbotAI()->SetCombatOrderByStr(orderStr, target);
    }
    return true;
}

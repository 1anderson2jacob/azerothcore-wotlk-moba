/*
 * This file is part of the AzerothCore Project. See AUTHORS file for Copyright information
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program. If not, see <http://www.gnu.org/licenses/>.
 */

// This is where scripts' loading functions should be declared:
// void MyExampleScript()
void AddSC_npc_moba_tower();
void AddSC_moba_tower_aggro();
void AddSC_npc_moba_creep();
void AddSC_npc_moba_neutral();
void AddSC_moba_respawn();
void AddSC_moba_recall();
void AddSC_moba_hud();
void AddSC_moba_kill_credit();
void AddSC_npc_moba_store();
void AddSC_moba_loot_rights();

// The name of this function should match:
// void Add${NameOfDirectory}Scripts()
void AddCustomScripts()
{
    AddSC_npc_moba_tower();
    AddSC_moba_tower_aggro();
    AddSC_npc_moba_creep();
    AddSC_npc_moba_neutral();
    AddSC_moba_respawn();
    AddSC_moba_recall();
    AddSC_moba_hud();
    AddSC_moba_kill_credit();
    AddSC_npc_moba_store();
    AddSC_moba_loot_rights();
}

//Copyright (c) 2016, 2017, 2018, 2019 Hitachi Vantara Corporation
//All Rights Reserved.
//
//   Licensed under the Apache License, Version 2.0 (the "License"); you may
//   not use this file except in compliance with the License. You may obtain
//   a copy of the License at
//
//         http://www.apache.org/licenses/LICENSE-2.0
//
//   Unless required by applicable law or agreed to in writing, software
//   distributed under the License is distributed on an "AS IS" BASIS, WITHOUT
//   WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied. See the
//   License for the specific language governing permissions and limitations
//   under the License.
//
//Authors: Allart Ian Vogelesang <ian.vogelesang@hitachivantara.com>
//
//Support:  "ivy" is not officially supported by Hitachi Vantara.
//          Contact one of the authors by email and as time permits, we'll help on a best efforts basis.

#include "ivy_engine.h"

std::pair<bool /*success*/, std::string /* message */>
ivy_engine::create_rollup(
      std::string attributeNameComboText
    , bool nocsvSection
    , bool quantitySection
    , bool maxDroopMaxtoMinIOPSSection
    , ivy_int quantity
    , ivy_float maxDroop
)
{
    {
        std::ostringstream o;
        o << "ivy engine API create_rollup(";
        o << "rollup_name = \"" << attributeNameComboText << "\"";
        o << ", nocsv = ";                          if (nocsvSection)                o << true; else o << "false";
        o << ", have_quantity_validation = ";       if (quantitySection)             o << true; else o << "false";
        o << ", have_max_IOPS_droop_validation = "; if (maxDroopMaxtoMinIOPSSection) o << true; else o << "false";
        o << ", quantity = " << quantity;
        o << ", max_droop = " << maxDroop;
        o << ")" << std::endl;
        std::cout << o.str();
        log(masterlogfile,o.str());
        log(ivy_engine_logfile,o.str());
    }

    if ( !haveHosts )
    {
        std::ostringstream o;
        o << "<Error> ivy engine API - attempt to create a rollup with no preceeding [hosts] statement to start up the ivy engine and discover the test configuration." << std::endl;
        return std::make_pair(false, o.str());
    }

    std::pair<bool,std::string> rc = rollups.addRollupType(attributeNameComboText, nocsvSection, quantitySection, maxDroopMaxtoMinIOPSSection, quantity, maxDroop);

    if (!rc.first) return rc;

    rollups.rebuild();

    return rc;
}


std::pair<bool /*success*/, std::string /* message */>
ivy_engine::delete_rollup(const std::string& attributeNameComboText)
{
    {
        std::ostringstream o;
        o << "ivy engine API delete_rollup("
            << "rollup_name = \"" << attributeNameComboText << "\""
            << ")" << std::endl;
        std::cout << o.str();
        log(masterlogfile,o.str());
        log(ivy_engine_logfile,o.str());
    }
    bool delete_all_not_all {false};

    if (attributeNameComboText.size()==0 || attributeNameComboText == "*") { delete_all_not_all = true;}

    if (delete_all_not_all)
    {
        // delete all rollups except "all"
        for (auto& pear : rollups.rollups)
        {
            if (stringCaseInsensitiveEquality("all", pear.first)) continue;

            auto rc = rollups.deleteRollup(pear.first);
            if ( !rc.first )
            {
                std::ostringstream o;
                o << "<Error> ivy engine API - for \"delete all rollups except the \'all\' rollup\" - failed trying to delete \""
                    << pear.first << "\" - " << rc.second << std::endl;
                log(m_s.masterlogfile,o.str());
                std::cout << o.str();
                return std::make_pair(false,o.str());
            }
        }
    }
    else
    {
        if (stringCaseInsensitiveEquality(std::string("all"),attributeNameComboText))
        {
            std::string msg = "\nI\'m so sorry, Dave, I cannot delete the \"all\" rollup.\n\n";
            log(m_s.masterlogfile,msg);
            std::cout << msg;
            return std::make_pair(false,msg);
        }

        AttributeNameCombo anc {};

        std::pair<bool,std::string>
            rc = anc.set(attributeNameComboText, &m_s.TheSampleLUN);
            // This is to handle spaces around attribute tokens & attribute names in quotes.

        if ( !rc.first )
        {
            std::ostringstream o;
            o << "<Error> ivy engine API - delete rollup for \"" << attributeNameComboText << "\" failed - " << rc.second << std::endl;
            log(m_s.masterlogfile,o.str());
            std::cout << o.str();
            return std::make_pair(false,o.str());
        }

        rc = m_s.rollups.deleteRollup(anc.attributeNameComboID);
        if ( !rc.first )
        {
            std::ostringstream o;
            o << "<Error> ivy engine API - delete rollup for \"" << attributeNameComboText << "\" failed - " << rc.second << std::endl;
            log(m_s.masterlogfile,o.str());
            std::cout << o.str();
            return std::make_pair(false,o.str());
        }
    }

    //m_s.need_to_rebuild_rollups=true; - removed so scripting language builtins see up to date data structures
    rollups.rebuild();

    return std::make_pair(true,std::string(""));
}


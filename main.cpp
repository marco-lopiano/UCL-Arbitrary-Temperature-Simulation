#include <algorithm>
#include <iostream>
#include <iomanip>
#include <random>
#include <vector>
#include <nlohmann/json.hpp>
#include <fstream>
#include <unordered_map>
#include <optional>
#include <cstdlib>

using namespace std;
using json = nlohmann::json;

struct Team {
    string name;
    string region;
    double rating;
    int startingPot;
    array<array<int, 2>, 4> pots = {{
        { -1, -1 },
        { -1, -1 },
        { -1, -1 },
        { -1, -1 }
    }};

};

struct Slot {
    int team;
    int pot;
    int ha;
};

// TODO: Possible optimizations:
// Candidate ordering (least-constraining value) -> Reduces recursion depth
// Forward checking (early deadlock detection)	-> Prevents deep wasted recursion
// Caching candidate count -> Avoids recomputing validity each call
// Weighted heuristics (regions, pots) -> Further improves MRV ordering
// Retry / alternate -> seeds	Safety net for unlucky seeds
// Parallel attempts -> For massive speedup if needed

class Calendar {
    private:
        mt19937 rng;
        bool DEBUG = false;

    public:
        json teamsData;
        vector<Team> teams;
        vector<string> pots = {"Pot1", "Pot2", "Pot3", "Pot4"};
        vector<Slot> slots;

        Calendar(json x, unsigned seed = 42, bool _debug=false) : rng(seed) {
            teamsData = x;
            DEBUG = _debug;
            initialize(teamsData);
        };

        optional<int> getPotIndex(string potstring) {
            auto it = find(pots.begin(), pots.end(), potstring);

            if (it != pots.end()) {
                int index = it - pots.begin();
                return index;
            }
            return nullopt;
        };

        void initialize(const json& teamsData) {
            for (auto& [key, value] : teamsData.items()) {
                for (auto& entry : value) {

                    auto _potindex = getPotIndex(key);
                    if (_potindex) {
                        Team t;
                        t.name = entry[0].get<std::string>();
                        t.rating = entry[1].get<double>();
                        t.region = entry[2].get<std::string>();
                        t.startingPot = *_potindex;
                        teams.push_back(t);
                    }
                }
            }
        };

        void listTeamsData() {
            for (Team t : teams) {
                cout << t.name << " - " << pots[t.startingPot] << " - " << t.rating << endl;
            }
        };

        // TODO: add color to team based on pot
        // TODO: add color to pot matches based on difficulty
        void printCalendar() {

            double globalTemperatureScore = 0.0;
            constexpr int teamNameWidth = 22;

            cout << "┌" << string(teamNameWidth + 4 * 7, '-') << "┐" << "\n";

            cout << "|" << setw(teamNameWidth) << left << "Team Name";

            for (int p = 0; p < 4; ++p) {
                cout << "| POT" << (p + 1) << " ";
            }
            cout << "|" << "\n";

            cout << "|" << setw(teamNameWidth) << left << " ";

            for (int p = 0; p < 4; ++p) {
                cout << "| H  A ";
            }
            cout << "|" << "\n";

            cout << "|" << string(teamNameWidth + 4 * 7, '-') << "|" << "\n";

            for (const Team& t : teams) {
                cout << "|" << setw(teamNameWidth) << left << t.name << "|";
                
                double teamTemp = 0.0;

                constexpr int potWidth = 3;
                for (int p = 0; p < 4; ++p) {

                    for (int ha=0; ha<2; ha++) {

                        double venueFactor = (ha == 0 ? 0.9 : 1.1);
                        double relative = sqrt(teams[t.pots[p][ha]].rating / t.rating);
                        teamTemp += teams[t.pots[p][ha]].rating * relative * venueFactor;

                    }
                    
                    cout << setw(potWidth) << t.pots[p][0] << setw(potWidth) << right << t.pots[p][1] << "|";

                    if (p == 3) {
                        cout << " -----> Team Temperature: " << teamTemp;
                        globalTemperatureScore += teamTemp;
                    };

                }
                cout << "\n";
            }
            cout << "└" << string(teamNameWidth + 4 * 7, '-') << "┘" << "\n";

            cout << "Global Temperature Score: " << globalTemperatureScore << endl;
        };

        vector<Slot> buildSlots() {
            vector<Slot> result;
            for (int t = 0; t < (int)teams.size(); ++t)
                for (int p = 0; p < 4; ++p)
                    for (int ha = 0; ha < 2; ++ha)
                        result.push_back({t, p, ha});
            return result; // don't write to this->slots
        }

        bool isValid(int t, int opp, int pot, int ha) {
            // Team cannot play itself
            if (t == opp)
                return false;

            // Opponent must belong to the pot
            if (teams[opp].startingPot != pot)
                return false;

            // Slot must be unassigned
            if (teams[t].pots[pot][ha] != -1)
                return false;

            // Opponents slot must be unassigned
            int oppPot = teams[t].startingPot;
            if (teams[opp].pots[oppPot][1 - ha] != -1)
                return false;

            // Region constraint still not implemented
            // need to add this in the initializer method and in the json file 
            if (!teams[t].region.empty() &&
                teams[t].region == teams[opp].region)
                return false;

            // No duplicate matches
            for (int p = 0; p < 4; ++p) {
                for (int h = 0; h < 2; ++h) {
                    if (teams[t].pots[p][h] == opp)
                        return false;
                }
            }

            // Optional UEFA-style soft constraint:
            // max 2 teams from same region
            if (!teams[t].region.empty()) {
                int count = 0;
                for (int p = 0; p < 4; ++p) {
                    for (int h = 0; h < 2; ++h) {
                        int other = teams[t].pots[p][h];
                        if (other != -1 &&
                            teams[other].region == teams[opp].region) {
                            count++;
                        }
                    }
                }
                if (count >= 2)
                    return false;
            }

            return true;
        };

        void placeMatch(int t, int opp, int pot, int ha) {
            if (DEBUG) {
                cout << " - Placing: "
                << teams[t].name << " vs "
                << teams[opp].name
                << " in pot " << pot
                << (ha == 0 ? " (home)\n" : " (away)\n");
            }

            teams[t].pots[pot][ha] = opp;
            teams[opp].pots[teams[t].startingPot][1 - ha] = t;
        };

        void undoMatch(int t, int opp, int pot, int ha) {
            if (DEBUG) {
                cout << " - Undoing: "
                << teams[t].name << " vs "
                << teams[opp].name
                << " in pot " << pot
                << (ha == 0 ? " (home)\n" : " (away)\n");
            }

            teams[t].pots[pot][ha] = -1;
            teams[opp].pots[teams[t].startingPot][1 - ha] = -1;
        };

        vector<int> getCandidates(int t, int pot, int ha) {
            vector<int> candidates;

            for (int opp = 0; opp < (int)teams.size(); ++opp) {
                if (isValid(t, opp, pot, ha))
                    candidates.push_back(opp);
            }

            shuffle(candidates.begin(), candidates.end(), rng);
            return candidates;
        };

        // Forward checking: after placing a match, verify no other unfilled slot
        // is left with zero candidates. If so, prune immediately.
        bool forwardCheck(const vector<Slot>& slots, int afterIdx) {
            for (int i = afterIdx; i < (int)slots.size(); ++i) {
                auto [t, pot, ha] = slots[i];
                if (teams[t].pots[pot][ha] != -1)
                    continue; // already filled, skip
                if (getCandidates(t, pot, ha).empty())
                    return false; // deadlock detected early
            }
            return true;
        }

        // Dynamic MRV: at each step, pick the unfilled slot with fewest candidates
        // among the remaining slots (starting from idx), swap it into position.
        bool solveSlots(vector<Slot>& slots, int idx = 0) {
            if (idx == (int)slots.size())
                return true;

            // Skip already-filled slots
            while (idx < (int)slots.size() &&
                teams[slots[idx].team].pots[slots[idx].pot][slots[idx].ha] != -1)
                ++idx;

            if (idx == (int)slots.size())
                return true;

            // Dynamic MRV: find the unfilled slot with fewest candidates
            int bestIdx = idx;
            int bestCount = INT_MAX;
            for (int i = idx; i < (int)slots.size(); ++i) {
                auto [t, pot, ha] = slots[i];
                if (teams[t].pots[pot][ha] != -1)
                    continue;
                int count = getCandidates(t, pot, ha).size();
                if (count < bestCount) {
                    bestCount = count;
                    bestIdx = i;
                    if (count == 0) break; // can't do worse
                }
            }

            // Swap best slot into current position
            swap(slots[idx], slots[bestIdx]);
            auto [t, pot, ha] = slots[idx];

            // Deadlock: no candidates for the most-constrained slot
            if (bestCount == 0)
                return false;

            auto candidates = getCandidates(t, pot, ha);
            for (int opp : candidates) {
                placeMatch(t, opp, pot, ha);

                // Forward check before recursing
                if (forwardCheck(slots, idx + 1) && solveSlots(slots, idx + 1))
                    return true;

                undoMatch(t, opp, pot, ha);
            }

            // Restore slot order on backtrack
            swap(slots[idx], slots[bestIdx]);
            return false;
        }

        int slotDifficulty(const Slot& s) {
            return getCandidates(s.team, s.pot, s.ha).size();
        }


        bool buildCalendar() {
            // Retry with seed jitter if solver gets stuck
            constexpr int MAX_ATTEMPTS = 20;
            auto baseSlots = buildSlots();

            for (int attempt = 0; attempt < MAX_ATTEMPTS; ++attempt) {
                // Reset all team assignments
                for (auto& t : teams)
                    t.pots = {{ {-1,-1}, {-1,-1}, {-1,-1}, {-1,-1} }};

                // Re-seed RNG with jitter so candidate shuffle varies
                rng.seed(rng()); 

                auto slots = baseSlots;
                if (solveSlots(slots)) {
                    if (attempt > 0)
                        cout << "Solved on attempt " << (attempt + 1) << "\n";
                    return true;
                }
            }
            return false;
        }

};


int main(int argc, char* argv[]) {

    ifstream f("teams.json");
    json teams = json::parse(f);

    int seed = 123456;
    string output = "Generating calendar with default seed: ";

    if (argc>=2) {
        int _seed = atoi(argv[1]);

        if (_seed!=0) {
            seed = _seed;
            output = "Generating calendar with seed: ";
        }

    }

    cout << endl;
    cout << output << seed << endl;
    cout << endl;

    Calendar cal(teams, seed);

    if (!cal.buildCalendar()) {
        cout << "Draw failed (deadlock)\n";
    } else {
        cal.printCalendar();
    }

    return 0;
}


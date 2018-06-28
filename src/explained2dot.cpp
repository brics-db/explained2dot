#include <iostream>
#include <iomanip>
#include <string>
#include <vector>
#include <map>
#include <algorithm>
#include <cctype>

#include <boost/algorithm/string.hpp>
#include <boost/algorithm/string/predicate.hpp>
#include <boost/filesystem.hpp>
#include <boost/iostreams/device/mapped_file.hpp>

#include "common.hpp"
#include "config.hpp"

namespace e2d {

///////////////////////////////
// EXPLAINED-2-DOT           //
///////////////////////////////
    const char* const FIND_ROOT = "function ";
    const size_t FIND_ROOT_LEN = strlen(FIND_ROOT);
    const char* const FIND_BARRIER = "barrier";
    const char* const FIND_ROOT_OPTIONS = "{";
    const char* const FIND_ROOT_OPTIONS_END = "}";
    const char* const FIND_ROOT_VARS = "(";
    const char* const FIND_ROOT_VARS_END = ")";
    const char* const FIND_SQL_MVC = "sql.mvc";
    const char* const SQL_ASSIGN = " := ";
    const size_t SQL_ASSIGN_LEN = strlen(SQL_ASSIGN);
    const char* const SQL_RESULT_SET = "sql.resultSet";
    const std::list<std::string> IGNORED_NAMES = {"nil", "true", "false"};
    const std::list<std::string> IGNORED_OPERATORS = {"querylog.define", "language.dataflow", "language.pass"};
    const std::list<std::string> IGNORED_LINES_BEGINS = {"+", "mal", "barrier ", "exit ", "end "};
    const std::map<std::string, std::string> COLORIZE_BG_PREFIX = { {"algebra", "cyan"}, {"aggr", "green"}, {"batcalc", "gold"}, {"group", "orangered"}, {"sql", "gainsboro"}, {"bat", "peachpuff"}};
    const std::map<std::string, std::string> COLORIZE_FG_PREFIX = { {"group", "white"}};

    bool is_number(
            const std::string& s) {
        return !s.empty() && find_if(s.begin(), s.end(), [](char c) {
            return !isdigit(c);}) == s.end();
    }

    bool ignore(
            std::string& name) {
        return ((boost::starts_with(name, "\"") && boost::ends_with(name, "\"")) || (boost::starts_with(name, "'") && boost::ends_with(name, "'"))
                || (find(IGNORED_NAMES.begin(), IGNORED_NAMES.end(), name) != IGNORED_NAMES.end()) || is_number(name));
    }

    const id_t INVALID_ID = static_cast<id_t>(-1);

    id_t nextID() {
        static id_t id = 0;
        return id++;
    }

    std::map<id_t, std::string> idsToNames;
    std::map<std::string, id_t> namesToIDs;
    std::map<id_t, std::string> idType;

    std::list<id_t> nodes;
    std::multimap<id_t, id_t> nodeIn;
    std::multimap<id_t, id_t> nodeOut;
    std::map<id_t, id_t> reassign;
    std::list<id_t> values;
    std::map<id_t, id_t> valueAssign;

/// FROM HERE taken from the answer from
/// http://stackoverflow.com/questions/216823/whats-the-best-way-to-trim-stdstring
    const char* const TRIM_CHARS = " \t\n\r\f\v |:";

// trim from end of string (right)

    inline std::string& rtrim(
            std::string& s,
            const char* t = TRIM_CHARS) {
        s.erase(s.find_last_not_of(t) + 1);
        return s;
    }

// trim from beginning of string (left)

    inline std::string& ltrim(
            std::string& s,
            const char* t = TRIM_CHARS) {
        s.erase(0, s.find_first_not_of(t));
        return s;
    }

// trim from both ends of string (left & right)

    inline std::string& trim(
            std::string& s,
            const char* t = TRIM_CHARS) {
        return ltrim(rtrim(s, t), t);
    }
/// UNTIL HERE
    const char* const TRIM_ARGS = " \t\n\r\f\v |;";

    std::string& replaceInString(
            std::string& s,
            char src,
            char dest) {
        if (src != dest) {
            size_t pos = s.find(src);
            while (pos != std::string::npos) {
                s.replace(pos, 1, 1, dest);
                pos = s.find(src, pos);
            }
        }
        return s;
    }

    size_t parse(
            id_t nodeID,
            std::string& s,
            bool isIn,
            size_t line) {
        size_t beg = 0, pos = 0, pos2 = 0, pos3 = 0;
        std::string name, type;
        bool hasType;
        if (s[0] == '(') {
            pos2 = std::string::npos;
            beg = 1;
            do {
                if (s[beg] == ')') {
                    break;
                } else if (s[beg] == ',') {
                    ++beg;
                }
                hasType = false;
                pos2 = s.find(',', beg);
                pos = s.find(':', beg);
                if (pos == std::string::npos && pos2 == std::string::npos) { // Simple type remaining
                    name = s.substr(beg, s.size() - beg - 1);
                } else if (pos < pos2) { // Type information
                    name = s.substr(beg, pos - beg);
                    pos3 = s.find('[', pos);
                    hasType = true;
                    if (pos3 < pos2) { // bat or other composite type
                        pos2 = s.find(']', pos3);
                        PRINT_ERROR_ON(pos2 == std::string::npos, "Did not find finalizing ']' on line " << line, __LINE__);
                        type = s.substr(pos + 1, pos2 - pos); // the whole bat type
                    } else { // Simple type with type information
                        type = s.substr(pos + 1, (pos2 == std::string::npos) ? (s.size() - pos - 2) : (pos2 - pos - 1));
                    }
                } else { // Simple type without type information
                    name = s.substr(beg, pos2 - beg);
                }
                trim(name);
                if (!::e2d::ignore(name)) {
                    if (isIn) {
                        auto iter = namesToIDs.find(name);
                        PRINT_ERROR_ON(iter == namesToIDs.end(), "No ID for name \"" << name << '"', __LINE__);
                        nodeIn.insert(std::make_pair(nodeID, iter->second)); // nodeIn[nodeID].push_back(iter->second);
                    } else {
                        id_t id = nextID();
                        idsToNames[id] = name;
                        namesToIDs[name] = id;
                        if (hasType) {
                            idType[id] = type;
                        }
                        nodeOut.insert(std::make_pair(nodeID, id)); //nodeOut[nodeID].push_back(id);
                    }
                }
                beg = pos2 == std::string::npos ? std::string::npos : pos2 + 1;
            } while ((beg != std::string::npos) && ((pos != std::string::npos) || (pos2 != std::string::npos)));
        } else {
            pos = s.find(':');
            id_t id = nextID();
            name = s.substr(0, pos);
            trim(name);
            idsToNames[id] = name;
            namesToIDs[name] = id;
            if (pos != std::string::npos) { // type information
                idType[id] = s.substr(pos + 1);
            }
            // isIn ? nodeIn[nodeID].push_back(id) : nodeOut[nodeID].push_back(id);
            isIn ? nodeIn.insert(std::make_pair(nodeID, id)) : nodeOut.insert(std::make_pair(nodeID, id));
        }
        return 0;
    }

    int main(
            int argc,
            char** argv) {
        config_t CONFIG;
        try {
            CONFIG.init(argc, argv);
        } catch (std::runtime_error & exc) {
            if (exc.what()) {
                std::cerr << exc.what() << std::endl;
            }
            CONFIG.HELP = true;
        }

        ///////////////////////
        // Argument checking //
        ///////////////////////
        if (argc == 1 || CONFIG.HELP) {
            boost::filesystem::path p(argv[0]);
            std::cerr << "Usage: " << p.filename() << " [-?|-h|--help] [--exclude-mvc|-m] [--compact|-c] [--exclude-result|-r] <explained file>\n";
            std::cerr << "\tDesigned for MonetDB!\n";
            std::cerr << "\t-?|-h|--help                  Display this help.\n";
            std::cerr << "\t--exclude-mvc|-m              Do not include the starting mvc node, its result, and respective edges in the graph.\n";
            std::cerr << "\t--compact|-c                  Generate a very compact graph.\n";
            std::cerr << "\t--exclude-result|-r           Exclude SQL result set and its descriptor BATs.\n";
            std::cerr << std::flush;
            return 1;
        }
        boost::filesystem::path pathIn(argv[argc - 1]);
        boost::iostreams::mapped_file file(pathIn);

        ///////////////////////////////////////
        // Read file and remove unused lines //
        ///////////////////////////////////////
        std::vector<std::string> linesVec;
        char* fileContents = file.data();
        boost::split(linesVec, fileContents, boost::is_any_of("\r\n"), boost::token_compress_on);
        std::vector<std::string> splitVec;
        for (auto iter = linesVec.begin(); iter != linesVec.end();) {
            std::string& s = *iter;
            trim(s);
            ++iter;
            while (iter != linesVec.end() && iter->size() > 0 && iter->at(0) == ':') {
                std::string& s2 = *iter++;
                std::string s3 = s2.substr(2, s2.size() - 4);
                s.append(trim(s3));
            }
            if (s.size()) {
                // Certain lines start with special words like "barrier" or "exit" and we don't need these lines for parsing, so only add if the line does NOT start with one of these special words.
                // bool isNotIgnoredLine = (find_if(IGNORED_LINES_BEGINS.begin(), IGNORED_LINES_BEGINS.end(), [&](string sComp) { return !boost::starts_with(s, sComp); }) == IGNORED_LINES_BEGINS.end());
                bool isNotIgnoredLine = true;
                for (auto iter : IGNORED_LINES_BEGINS) {
                    isNotIgnoredLine &= (s.find(iter) > 0);
                }
                // Also ignore lines which contain special operators
                // bool isNotIgnoredOp = (find_if(IGNORED_OPERATORS.begin(), IGNORED_OPERATORS.end(), [&](string sComp) { return s.find(sComp) == string::npos; }) == IGNORED_OPERATORS.end());
                bool isNotIgnoredOp = true;
                for (auto iter : IGNORED_OPERATORS) {
                    isNotIgnoredOp &= (s.find(iter) == std::string::npos);
                }
                if (isNotIgnoredLine && isNotIgnoredOp) {
                    splitVec.push_back(s);
                }
            }
        }
        const size_t szSplitVec = splitVec.size();

#if defined(VERBOSE)
        size_t i = 0;
        for (auto s : splitVec) {
            cout << "// [VERBOSE] " << (++i) << ": " << s << endl;
        }
#endif

        size_t end, pos;

        ////////////////////////////////////////////////////
        // Retreive function name, options, and variables //
        ////////////////////////////////////////////////////
        std::string &s = trim(splitVec[0]);
        if (s.find("auto commit") != std::string::npos) {
            s = trim(splitVec[1]);
        }
        pos = s.find(FIND_ROOT);
        PRINT_ERROR_ON(pos == std::string::npos, "Could not find root node \"" << FIND_ROOT << "\" in String\n\t" << s, __LINE__);
        pos += FIND_ROOT_LEN;
        end = s.find(FIND_ROOT_OPTIONS, pos);
        std::string rootName;
        if (end == std::string::npos) {
            // No options
            end = s.find(FIND_ROOT_VARS, pos);
            PRINT_ERROR_ON(end == std::string::npos, "Could not find variables section of root function", __LINE__);
            rootName = s.substr(pos, end - pos);
        } else {
            rootName = s.substr(pos, end - pos);
            pos = end + 1;
            end = s.find(FIND_ROOT_OPTIONS_END, pos);
            PRINT_ERROR_ON(end == std::string::npos, "Could not determine name of root node, while searching for \"" << FIND_ROOT_OPTIONS << "\" in String\n\t" << s, __LINE__);
        }
        trim(rootName);
        pos = s.find(FIND_ROOT_VARS, end);
        PRINT_ERROR_ON(pos == std::string::npos, "Could not find variables section of root function", __LINE__);
        ++pos;
        end = s.find(FIND_ROOT_VARS_END, pos);
        PRINT_ERROR_ON(pos == std::string::npos, "Root function variables do not terminate on the same line. This is not yet supported :-(", __LINE__);
        std::vector<std::string> variables;
        std::string variablesString = s.substr(pos, end - pos);
        boost::split(variables, variablesString, boost::is_any_of(","), boost::token_compress_on);
        for (auto sub : variables) {
            pos = sub.find(':');
            id_t id = nextID();
            idsToNames[id] = sub.substr(0, pos);
            namesToIDs[sub.substr(0, pos)] = id;
            idType[id] = sub.substr(pos + 1);
        }
#if defined(DEBUG) or defined(VERBOSE)
        cout << "// [DEBUG] rootName = \"" << rootName << "\"\n";
        cout << "// [DEBUG] variables: {";
        for (auto it : namesToIDs) {
            cout << '[' << it.first << ';' << idType[it.second] << ']';
        }
        cout << '}' << endl;
#endif

        /////////////////////////////////////////
        // Find start of actual execution part //
        /////////////////////////////////////////
        /*
         ssize_t sqlStart = -1;
         id_t mvcID = INVALID_ID;
         std::string nameMVC;
         for (size_t i = 1; i < szSplitVec; ++i) {
         std::string& s = trim(splitVec[i]);
         if ((pos = s.find(FIND_SQL_MVC)) != std::string::npos) {
         sqlStart = i + 1;
         nameMVC = s.substr(0, pos - SQL_ASSIGN_LEN);
         mvcID = nextID();
         idsToNames[mvcID] = nameMVC;
         namesToIDs[nameMVC] = mvcID;
         #if defined(DEBUG) or defined(VERBOSE)
         cout << "// [DEBUG] mvc() = " << nameMVC << endl;
         #endif
         break;
         }
         }
         myErrorOn(sqlStart == -1, "sql Start \"sql.mvc()\" not found!", __LINE__);

         #if defined(DEBUG) or defined(VERBOSE)
         s = splitVec[sqlStart];
         cout << "// [DEBUG] SQL start @ line " << (sqlStart+1) << ":\n//\t" << (sqlStart+1) << ": \"" << trim(s) << "\"\n";
         #endif
         */

        // start graph output
        std::cout << "digraph " << pathIn.stem() << " {\n\tnode [shape=box];\n";

        id_t mvcID = INVALID_ID;

        // Build graph by iterating over all lines.
        std::vector<std::string> splitVecSub;
        for (size_t i = 1; i < szSplitVec; ++i) {
            std::string& s = trim(splitVec[i]);
            pos = s.find(SQL_ASSIGN);
            bool isSqlResultSet = boost::starts_with(s, SQL_RESULT_SET);
            if (!isSqlResultSet && (pos == std::string::npos)) {
#if defined(DEBUG)
                std::cout << "// [DEBUG] No assignment on line " << (i + 1) << endl;
#endif
                continue; // TODO: this is very clumsy handling of the bottom lines!
            }
            std::string left = isSqlResultSet ? "" : s.substr(0, pos);
            std::string right = isSqlResultSet ? s : s.substr(pos + SQL_ASSIGN_LEN);
#if defined(VERBOSE)
            std::cout << "// [VERBOSE] left: \"" << left << "\" right :\"" << right << "\"\n";
#endif
            // check node name etc.
            pos = right.find('(');
            std::string nodeLabel = right.substr(0, pos);
            id_t nodeID = nextID();
            if (pos == std::string::npos) {
                trim(nodeLabel, TRIM_ARGS);
                // This is a reassignment (A_x -> A_y) or value assignment (PseudoNode -> A_x)
                idsToNames[nodeID] = left;
                namesToIDs[left] = nodeID;
                if (nodeLabel.find('@') == std::string::npos) {
                    // Reassignment
                    PRINT_ERROR_ON(namesToIDs.find(nodeLabel) == namesToIDs.end(), " No ID for argument \"" << nodeLabel << '"', __LINE__);
                    id_t srcID = namesToIDs[nodeLabel];
                    reassign[srcID] = nodeID;
                } else {
                    // Simple value assignment
                    id_t valueID = nextID();
                    values.push_back(valueID);
                    idsToNames[valueID] = right;
                    valueAssign[valueID] = nodeID;
                }
            } else {
                nodes.push_back(nodeID);
                std::string nodeArgs = right.substr(pos, right.size() - pos - 1);
                trim(nodeArgs, TRIM_ARGS);
                replaceInString(nodeArgs, '"', '\'');

                bool isMVCNode = nodeLabel.compare(FIND_SQL_MVC) == 0;
                if (isMVCNode) {
                    mvcID = nodeID;
                }

                if (!CONFIG.EXCLUDE_MVC || !isMVCNode) {
                    // PRINT node
                    std::cout << "\tN" << nodeID << " [label=\"" << nodeLabel << "\\n" << nodeArgs << "\"";
                    if ((pos = nodeLabel.find('.')) != std::string::npos) {
                        for (auto p : COLORIZE_BG_PREFIX) {
                            if (nodeLabel.compare(0, pos, p.first) == 0) {
                                std::cout << " style=filled fillcolor=" << p.second;
                                break;
                            }
                        }
                        for (auto p : COLORIZE_FG_PREFIX) {
                            if (nodeLabel.compare(0, pos, p.first) == 0) {
                                std::cout << " fontcolor=" << p.second;
                                break;
                            }
                        }
                    }
                    std::cout << "];\n";
                }

                // first parse arguments = right (in) then return values = left (out)
                PRINT_ERROR_ON(parse(nodeID, nodeArgs, true, i + 1) != 0, " parse node arguments on line " << (i + 1), __LINE__);
                if (!isSqlResultSet) {
                    PRINT_ERROR_ON(parse(nodeID, left, false, i + 1) != 0, " parse node return values", __LINE__);
                }
            }
        }

        // exclude nodes
        if (CONFIG.EXCLUDE_MVC) {
            // don't throw an error since we want to ignore it anyways
            PRINT_WARN_ON(mvcID == INVALID_ID, "MVC node shall be excluded, but no " << FIND_SQL_MVC << " node found!", __LINE__);
            id_t mvcOutID = nodeOut.find(mvcID)->second;
            auto iter = nodeIn.begin();
            while (iter != nodeIn.end()) {
                if (iter->second == mvcOutID) {
                    iter = nodeIn.erase(iter);
                } else {
                    ++iter;
                }
            }
            // for (auto p : nodeIn) {
            //     if (find(p.second.begin(), p.second.end(), mvcOutID) != p.second.end()) {
            //         cerr << "Not all references to MVC were deleted! Current NodeID=" << p.first << endl;
            //         exit(1);
            //     }
            // }
            // nodeIn.erase(mvcID);
            // nodeIn.erase(mvcOutID);
            // nodeOut.erase(mvcID);
            // nodeOut.erase(mvcOutID);
            // idsToNames.erase(mvcID);
        }

        // print values
        if (values.size()) {
            std::cout << "\n\tnode [shape=star];\n";
            for (auto id : values) {
                std::cout << "\tV" << id << " [label=\"" << idsToNames[id] << "\"];\n";
            }
        }

        // Generate unique set of arguments
        std::map<id_t, id_t> argsMap;
        for (auto iter : nodeIn) {
            // for (auto itIn : itNode.second) {
            argsMap[iter.second] = iter.second;
            // }
        }
        for (auto iter : nodeOut) {
            // for (auto itOut : itNode.second) {
            argsMap[iter.second] = iter.second;
            // }
        }
        for (auto itReassign : reassign) {
            argsMap[itReassign.first] = itReassign.second;
        }

        // PRINT argument nodes
#if defined(DEBUG)
        std::cout << "\n\t// [DEBUG]";
        for (auto it : argsMap) {
            std::cout << " A" << it.first;
        }
#endif

        std::cout << "\n\tnode [shape=ellipse]\n";
        for (auto it : argsMap) {
            std::cout << "\tA" << it.first << " [label=\"" << idsToNames[it.first] << "\\n" << idType[it.first] << "\"];\n";
        }

        // PRINT Incoming archs
        std::cout << "\n";
        for (auto iter : nodeIn) {
            // for (auto itIn : iter.second) {
            std::cout << "\tA" << iter.second << " -> N" << iter.first << ";\n";
            // }
        }
        // PRINT Outgoing archs
        std::cout << "\n";
        for (auto iter : nodeOut) {
            // for (auto itOut : iter.second) {
            std::cout << "\tN" << iter.first << " -> A" << iter.second << ";\n";
            // }
        }
        // PRINT reassignments
        if (reassign.size()) {
            std::cout << "\n";
            for (auto itReassign : reassign) {
                std::cout << "\tA" << itReassign.first << " -> A" << itReassign.second << ";\n";
            }
        }
        // PRINT value assignemnts
        if (values.size()) {
            std::cout << "\n";
            for (auto id : values) {
                std::cout << "\tV" << id << " -> A" << valueAssign[id] << ";\n";
            }
        }

        std::cout << "}" << std::endl;

#if defined(DEBUG) or defined(VERBOSE)
        std::cout << "// [DEBUG] All found variables / BAT's / etc.: {";
        for (auto it : namesToIDs) {
            std::cout << '[' << it.first << ';' << idType[it.second] << ']';
        }
        std::cout << '}' << endl;
#endif

        return 0;
    }

}

int main(
        int argc,
        char ** argv) {
    return e2d::main(argc, argv);
}

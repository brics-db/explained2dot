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

using namespace std;



///////////////////////////////
// CMDLINE ARGUMENT PARSING  //
///////////////////////////////
map<string, size_t> cmdIntArgs = {
};

map<string, string> cmdStrArgs = {
};

map<string, bool> cmdBoolArgs = {
    {"--help", false},
    {"-h", false},
    {"-?", false},
    {"--exclude-mvc", false}
};

enum cmdargtype_t {
    argint, argstr, argbool
};

map<string, cmdargtype_t> cmdArgTypes = {
    {"--help", argbool},
    {"-h", argbool},
    {"-?", argbool},
    {"--exclude-mvc", argbool}
};

struct config_t {
    bool DO_EXCLUDE_MVC;
    bool HELP;

    config_t() : DO_EXCLUDE_MVC(cmdBoolArgs["--exclude-mvc"]), HELP(cmdBoolArgs["--help"] | cmdBoolArgs["-h"] | cmdBoolArgs["-?"]) {
    }
};

size_t parseint(const string& name, int argc, char** argv, int nArg) {
    if (argv == nullptr) {
        stringstream ss;
        ss << "Parameter \"" << name << "\" requires an additional integer value!";
        throw runtime_error(ss.str());
    }
    string str(argv[nArg]);
    size_t idx = string::npos;
    size_t value;
    try {
        value = stoul(str, &idx);
    } catch (invalid_argument& exc) {
        stringstream ss;
        ss << "Value for parameter \"" << name << "\" is not an integer (is \"" << str << "\")!";
        throw runtime_error(ss.str());
    }
    if (idx < str.length()) {
        stringstream ss;
        ss << "Value for parameter \"" << name << "\" is not an integer (is \"" << str << "\")!";
        throw runtime_error(ss.str());
    }
    cmdIntArgs[name] = value;
    return 1;
}

size_t parsestr(const string& name, int argc, char** argv, int nArg) {
    if (argv == nullptr) {
        stringstream ss;
        ss << "Parameter \"" << name << "\" requires an additional string value!";
        throw runtime_error(ss.str());
    }
    cmdStrArgs[name] = argv[nArg];
    return 1;
}

size_t parsebool(const string& name, __attribute__((unused)) int argc, __attribute__((unused)) char** argv, __attribute__((unused)) int nArg) {
    cmdBoolArgs[name] = true;
    return 0;
}

size_t parsearg(const string& name, cmdargtype_t argtype, int argc, char** argv, int nArg) {
    switch (argtype) {
        case argint:
            return parseint(name, argc, argv, nArg);

        case argstr:
            return parsestr(name, argc, argv, nArg);

        case argbool:
            return parsebool(name, argc, argv, nArg);
    }
    return 0;
}

config_t initConfig(int argc, char** argv) {
    if (argc > 1) {
        for (int nArg = 1; nArg < argc; ++nArg) {
            if (argv[nArg][0] != '-') {
                continue;
            }
            bool recognized = false;
            for (auto p : cmdArgTypes) {
                if (p.first.compare(argv[nArg]) == 0) {
                    recognized = true;
                    if ((nArg + 1) >= argc) {
                        stringstream ss;
                        ss << "Required value for parameter \"" << argv[nArg] << "\" missing!";
                        throw runtime_error(ss.str());
                    }
                    nArg += parsearg(p.first, p.second, argc, (nArg + 1) <= argc ? argv : nullptr, nArg + 1);
                    break;
                }
            }
            if (!recognized) {
                stringstream ss;
                ss << "Parameter \"" << argv[nArg] << "\" is unknown!";
                throw runtime_error(ss.str());
            }
        }
    }
    return config_t();
}




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
const list<string> IGNORED_NAMES = {"nil", "true", "false"};
const list<string> IGNORED_OPERATORS = {"querylog.define", "language.dataflow", "language.pass"};
const list<string> IGNORED_LINES_BEGINS = {"+", "mal", "barrier ", "exit ", "end "};
const map<string, string> COLORIZE_PREFIX = {
    {"algebra", "cyan"},
    {"aggr", "green"},
    {"batcalc", "gold"},
    {"group", "orangered"}
};

bool is_number(const string& s) {
    return !s.empty() && find_if(s.begin(), s.end(), [](char c) {
        return !isdigit(c); }) == s.end();
}

bool ignore(string& name) {
    return ((boost::starts_with(name, "\"") && boost::ends_with(name, "\""))
            || (boost::starts_with(name, "'") && boost::ends_with(name, "'"))
            || (find(IGNORED_NAMES.begin(), IGNORED_NAMES.end(), name) != IGNORED_NAMES.end())
            || is_number(name));
}

const id_t INVALID_ID = static_cast<id_t> (-1);

id_t nextID() {
    static id_t id = 0;
    return id++;
}

map<id_t, string> idsToNames;
map<string, id_t> namesToIDs;
map<id_t, string> idType;

list<id_t> nodes;
multimap<id_t, id_t> nodeIn;
multimap<id_t, id_t> nodeOut;
map<id_t, id_t> reassign;
list<id_t> values;
map<id_t, id_t> valueAssign;

#define myErrorOn(predicate, message, line)   \
if (predicate) {                                \
        cerr << "[ERROR @ " << __FILE__ << ':' << line << "] " << message << endl;      \
        return line;                              \
}

/// FROM HERE taken from the answer from
/// http://stackoverflow.com/questions/216823/whats-the-best-way-to-trim-stdstring
const char* const TRIM_CHARS = " \t\n\r\f\v |";
const char* const TRIM_CHARS2 = " \t\n\r\f\v :";

// trim from end of string (right)

inline std::string& rtrim(std::string& s, const char* t = TRIM_CHARS) {
    s.erase(s.find_last_not_of(t) + 1);
    return s;
}

// trim from beginning of string (left)

inline std::string& ltrim(std::string& s, const char* t = TRIM_CHARS) {
    s.erase(0, s.find_first_not_of(t));
    return s;
}

// trim from both ends of string (left & right)

inline std::string& trim(std::string& s, const char* t = TRIM_CHARS) {
    return ltrim(rtrim(s, t), t);
}
/// UNTIL HERE
const char* const TRIM_ARGS = " \t\n\r\f\v |;";

string& replaceInString(string& s, char src, char dest) {
    if (src != dest) {
        size_t pos = s.find(src);
        while (pos != string::npos) {
            s.replace(pos, 1, 1, dest);
            pos = s.find(src, pos);
        }
    }
    return s;
}

size_t parse(id_t nodeID, string& s, bool isIn, size_t line) {
    size_t beg = 0, pos = 0, pos2 = 0, pos3 = 0;
    string name, type;
    bool hasType;
    if (s[0] == '(') {
        pos2 = string::npos;
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
            if (pos == string::npos && pos2 == string::npos) { // Simple type remaining
                name = s.substr(beg, s.size() - beg - 1);
            } else if (pos < pos2) { // Type information
                name = s.substr(beg, pos - beg);
                pos3 = s.find('[', pos);
                hasType = true;
                if (pos3 < pos2) { // bat or other composite type
                    pos2 = s.find(']', pos3);
                    myErrorOn(pos2 == string::npos, "Did not find finalizing ']' on line " << line, __LINE__);
                    type = s.substr(pos + 1, pos2 - pos); // the whole bat type
                } else { // Simple type with type information
                    type = s.substr(pos + 1, (pos2 == string::npos) ? (s.size() - pos - 2) : (pos2 - pos - 1));
                }
            } else { // Simple type without type information
                name = s.substr(beg, pos2 - beg);
            }
            if (!::ignore(name)) {
                if (isIn) {
                    auto iter = namesToIDs.find(name);
                    myErrorOn(iter == namesToIDs.end(), "No ID for name \"" << name << '"', __LINE__);
                    nodeIn.insert(make_pair(nodeID, iter->second)); // nodeIn[nodeID].push_back(iter->second);
                } else {
                    id_t id = nextID();
                    idsToNames[id] = name;
                    namesToIDs[name] = id;
                    if (hasType) {
                        idType[id] = type;
                    }
                    nodeOut.insert(make_pair(nodeID, id)); //nodeOut[nodeID].push_back(id);
                }
            }
            beg = pos2 == string::npos ? string::npos : pos2 + 1;
        } while ((beg != string::npos) && ((pos != string::npos) || (pos2 != string::npos)));
    } else {
        pos = s.find(':');
        id_t id = nextID();
        name = s.substr(0, pos);
        idsToNames[id] = name;
        namesToIDs[name] = id;
        if (pos != string::npos) { // type information
            idType[id] = s.substr(pos + 1);
        }
        // isIn ? nodeIn[nodeID].push_back(id) : nodeOut[nodeID].push_back(id);
        isIn ? nodeIn.insert(make_pair(nodeID, id)) : nodeOut.insert(make_pair(nodeID, id));
    }
    return 0;
}

int main(int argc, char** argv) {
    config_t CONFIG;
    try {
        CONFIG = initConfig(argc, argv);
    } catch (runtime_error&) {
        CONFIG.HELP = true;
    }

    ///////////////////////
    // Argument checking //
    ///////////////////////
    if (argc == 1 || CONFIG.HELP) {
        boost::filesystem::path p(argv[0]);
        cerr << "Usage: " << p.filename() << " [-?|-h|--help] [--exclude-mvc] <explained file>\n\tOnly works for MonetDB!" << endl;
        return 1;
    }
    boost::filesystem::path pathIn(argv[argc - 1]);
    boost::iostreams::mapped_file file(pathIn);

    ///////////////////////////////////////
    // Read file and remove unused lines //
    ///////////////////////////////////////
    vector<string> linesVec;
    char* fileContents = file.data();
    boost::split(linesVec, fileContents, boost::is_any_of("\r\n"), boost::token_compress_on);
    vector<string> splitVec;
    for (auto iter = linesVec.begin(); iter != linesVec.end();) {
        string& s = *iter;
        trim(s);
        ++iter;
        while (iter != linesVec.end() && iter->size() > 0 && iter->at(0) == ':') {
            string& s2 = *iter++;
            string s3 = s2.substr(2, s2.size() - 4);
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
                isNotIgnoredOp &= (s.find(iter) == string::npos);
            }
            if (isNotIgnoredLine && isNotIgnoredOp)
                splitVec.push_back(s);
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
    string &s = trim(splitVec[0]);
    if (s.find("auto commit") != string::npos) {
        s = trim(splitVec[1]);
    }
    pos = s.find(FIND_ROOT);
    myErrorOn(pos == string::npos, "Could not find root node \"" << FIND_ROOT << "\" in String\n\t" << s, __LINE__);
    pos += FIND_ROOT_LEN;
    end = s.find(FIND_ROOT_OPTIONS, pos);
    myErrorOn(end == string::npos, "Could not determine name of root node, while searching for \"" << FIND_ROOT_OPTIONS << "\" in String\n\t" << s, __LINE__);
    string rootName = s.substr(pos, end - pos);
    pos = s.find(FIND_ROOT_VARS, end);
    myErrorOn(pos == string::npos, "Could not find variables section of root function", __LINE__);
    ++pos;
    end = s.find(FIND_ROOT_VARS_END, pos);
    myErrorOn(pos == string::npos, "Root function variables do not terminate on the same line. This is not yet supported :-(", __LINE__);
    vector<string> variables;
    string variablesString = s.substr(pos, end - pos);
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
    string nameMVC;
    for (size_t i = 1; i < szSplitVec; ++i) {
        string& s = trim(splitVec[i]);
        if ((pos = s.find(FIND_SQL_MVC)) != string::npos) {
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
    cout << "digraph " << pathIn.stem() << " {\n\tnode [shape=box];\n";

    id_t mvcID = INVALID_ID;

    // Build graph by iterating over all lines.
    vector<string> splitVecSub;
    for (size_t i = 1; i < szSplitVec; ++i) {
        string& s = trim(splitVec[i]);
        pos = s.find(SQL_ASSIGN);
        bool isSqlResultSet = boost::starts_with(s, SQL_RESULT_SET);
        if (!isSqlResultSet && (pos == string::npos)) {
#if defined(DEBUG)
            cout << "// [DEBUG] No assignment on line " << (i + 1) << endl;
#endif
            continue;
        }
        string left = isSqlResultSet ? "" : s.substr(0, pos);
        string right = isSqlResultSet ? s : s.substr(pos + SQL_ASSIGN_LEN);
#if defined(VERBOSE)
        cout << "// [VERBOSE] left: \"" << left << "\" right :\"" << right << "\"\n";
#endif
        // check node name etc.
        pos = right.find('(');
        string nodeLabel = right.substr(0, pos);
        id_t nodeID = nextID();
        if (pos == string::npos) {
            trim(nodeLabel, TRIM_ARGS);
            // This is a reassignment (A_x -> A_y) or value assignment (PseudoNode -> A_x)
            idsToNames[nodeID] = left;
            namesToIDs[left] = nodeID;
            if (nodeLabel.find('@') == string::npos) {
                // Reassignment
                myErrorOn(namesToIDs.find(nodeLabel) == namesToIDs.end(), " No ID for argument \"" << nodeLabel << '"', __LINE__);
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
            string nodeArgs = right.substr(pos, right.size() - pos - 1);
            trim(nodeArgs, TRIM_ARGS);
            replaceInString(nodeArgs, '"', '\'');

            if (nodeLabel.compare(FIND_SQL_MVC) == 0) {
                mvcID = nodeID;
            }

            // PRINT node
            cout << "\tN" << nodeID << " [label=\"" << nodeLabel << "\\n" << nodeArgs << "\"";
            if ((pos = nodeLabel.find('.')) != string::npos) {
                for (auto p : COLORIZE_PREFIX) {
                    if (nodeLabel.compare(0, pos, p.first) == 0) {
                        cout << " style=filled fillcolor=" << p.second;
                        break;
                    }
                }
            }
            cout << "];\n";

            // first parse arguments = right (in) then return values = left (out)
            myErrorOn(parse(nodeID, nodeArgs, true, i + 1) != 0, " parse node arguments on line " << (i + 1), __LINE__);
            if (!isSqlResultSet) {
                myErrorOn(parse(nodeID, left, false, i + 1) != 0, " parse node return values", __LINE__);
            }
        }
    }

    // exclude nodes
    if (CONFIG.DO_EXCLUDE_MVC) {
        // don't throw an error since we want to ignore it anyways
        myErrorOn(mvcID == INVALID_ID, "no " << FIND_SQL_MVC << " node found!", __LINE__);
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
        cout << "\n\tnode [shape=star];\n";
        for (auto id : values) {
            cout << "\tV" << id << " [label=\"" << idsToNames[id] << "\"];\n";
        }
    }

    // Generate unique set of arguments
    map<id_t, id_t> argsMap;
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
    cout << "\n\t// [DEBUG]";
    for (auto it : argsMap) {
        cout << " A" << it.first;
    }
#endif

    cout << "\n\tnode [shape=ellipse]\n";
    for (auto it : argsMap) {
        cout << "\tA" << it.first << " [label=\"" << idsToNames[it.first] << "\\n" << idType[it.first] << "\"];\n";
    }

    // PRINT Incoming archs
    cout << "\n";
    for (auto iter : nodeIn) {
        // for (auto itIn : iter.second) {
        cout << "\tA" << iter.second << " -> N" << iter.first << ";\n";
        // }
    }
    // PRINT Outgoing archs
    cout << "\n";
    for (auto iter : nodeOut) {
        // for (auto itOut : iter.second) {
        cout << "\tN" << iter.first << " -> A" << iter.second << ";\n";
        // }
    }
    // PRINT reassignments
    if (reassign.size()) {
        cout << "\n";
        for (auto itReassign : reassign) {
            cout << "\tA" << itReassign.first << " -> A" << itReassign.second << ";\n";
        }
    }
    // PRINT value assignemnts
    if (values.size()) {
        cout << "\n";
        for (auto id : values) {
            cout << "\tV" << id << " -> A" << valueAssign[id] << ";\n";
        }
    }

    cout << "}" << endl;

#if defined(DEBUG) or defined(VERBOSE)
    cout << "// [DEBUG] All found variables / BAT's / etc.: {";
    for (auto it : namesToIDs) {
        cout << '[' << it.first << ';' << idType[it.second] << ']';
    }
    cout << '}' << endl;
#endif

    return 0;
}


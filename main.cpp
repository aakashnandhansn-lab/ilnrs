#include <algorithm>
#include <ctime>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>
using namespace std;

namespace ilnrs {

enum class Role { Admin, Elder, Researcher, Public };
enum class Tier { Open = 1, CommunityOnly = 2, Restricted = 3 };

string roleName(Role r) {
    switch (r) {
        case Role::Admin:      return "Administrator";
        case Role::Elder:      return "Elder";
        case Role::Researcher: return "Youth Researcher";
        default:               return "Public Explorer";
    }
}

string tierName(Tier t) {
    switch (t) {
        case Tier::Open:          return "Public";
        case Tier::CommunityOnly: return "Community-Only";
        default:                  return "Restricted";
    }
}

Tier tierFromInt(int n) {
    if (n < 1 || n > 3)
        throw invalid_argument("Access level must be 1 to 3.");
    return static_cast<Tier>(n);
}

string trim(const string& s) {
    size_t a = s.find_first_not_of(" \t\r\n");
    if (a == string::npos) return "";
    return s.substr(a, s.find_last_not_of(" \t\r\n") - a + 1);
}

string lower(string s) {
    transform(s.begin(), s.end(), s.begin(), ::tolower);
    return s;
}

string stamp(bool withTime, int addDays = 0) {
    time_t t = time(nullptr) + addDays * 86400L;
    char buf[32];
    strftime(buf, sizeof buf, withTime ? "%Y-%m-%d %H:%M:%S" : "%Y-%m-%d",
             localtime(&t));
    return buf;
}

void checkText(const string& s, const string& field) {
    if (s.empty()) throw invalid_argument(field + " cannot be empty.");
    if (s.find('|') != string::npos)
        throw invalid_argument("The character '|' is not allowed.");
}

vector<string> split(const string& line, char sep) {
    vector<string> parts;
    string item;
    stringstream ss(line);
    while (getline(ss, item, sep)) parts.push_back(item);
    return parts;
}

class User {
    string name;
    Role role;
public:
    User(const string& n, Role r) : name(n), role(r) {}
    const string& getName() const { return name; }
    Role getRole() const { return role; }
};

class Record {
    int id;
    string name;
    double latitude, longitude;
public:
    Record(int id, const string& name, double lat, double lon)
        : id(id), name(trim(name)), latitude(lat), longitude(lon) {
        checkText(this->name, "Place name");
        if (lat < -90 || lat > 90)
            throw invalid_argument("Latitude must be between -90 and 90.");
        if (lon < -180 || lon > 180)
            throw invalid_argument("Longitude must be between -180 and 180.");
    }
    virtual ~Record() {}
    int getId() const { return id; }
    const string& getName() const { return name; }
    double getLatitude() const { return latitude; }
    double getLongitude() const { return longitude; }
    virtual void display() const {
        cout << "[" << id << "] " << name << " (" << fixed << setprecision(4)
             << latitude << ", " << longitude << ")" << endl;
    }
};

class PlaceNameRecord : public Record {
    string colonialName;
    Tier tier;
    string owner;
public:
    PlaceNameRecord(int id, const string& name, const string& colonial,
                    double lat, double lon, Tier tier, const string& owner)
        : Record(id, name, lat, lon), colonialName(trim(colonial)),
          tier(tier), owner(owner) {
        if (colonialName.empty()) colonialName = "Not recorded";
        checkText(colonialName, "Colonial name");
    }
    const string& getColonialName() const { return colonialName; }
    Tier getTier() const { return tier; }
    const string& getOwner() const { return owner; }
    void display() const override {
        Record::display();
        cout << "    Colonial name : " << colonialName << endl
             << "    Access level  : " << tierName(tier) << endl
             << "    Recorded by   : " << owner << endl;
    }
};

class AccessGrant {
    int id, recordId;
    string user, status, expires;
public:
    AccessGrant(int id, int recordId, const string& user,
                const string& status, const string& expires)
        : id(id), recordId(recordId), user(user), status(status),
          expires(expires) {}
    int getId() const { return id; }
    int getRecordId() const { return recordId; }
    const string& getUser() const { return user; }
    const string& getStatus() const { return status; }
    const string& getExpires() const { return expires; }
    void approve(int days) {
        status = "Active";
        expires = stamp(false, days);
    }
    void deny() { status = "Denied"; }
    bool isActive() const {
        return status == "Active" && expires >= stamp(false);
    }
};

struct Observation {
    int recordId;
    string type;
    double value;
    string date, contributor;
};

class IlnrsService {
    vector<User> users;
    vector<PlaceNameRecord> records;
    vector<AccessGrant> grants;
    vector<Observation> observations;
    const string recordFile = "records.txt", grantFile = "grants.txt";
    const string obsFile = "observations.txt", auditFile = "audit.txt";

    void log(const User& u, const string& action, const string& detail) {
        ofstream out(auditFile, ios::app);
        if (!out) {
            cout << "File saving error: " << auditFile << endl;
            return;
        }
        out << stamp(true) << "|" << u.getName() << "|" << action << "|"
            << detail << endl;
    }

    PlaceNameRecord& findRecord(int id) {
        for (auto& r : records)
            if (r.getId() == id) return r;
        throw invalid_argument("No record with ID " + to_string(id) + ".");
    }

    bool canView(const User& u, const PlaceNameRecord& r) const {
        if (r.getTier() == Tier::Open) return true;
        if (r.getTier() == Tier::CommunityOnly)
            return u.getRole() != Role::Public;
        if (u.getName() == r.getOwner()) return true;
        for (const auto& g : grants)
            if (g.getRecordId() == r.getId() && g.getUser() == u.getName() &&
                g.isActive())
                return true;
        return false;
    }

    void saveRecords() {
        ofstream out(recordFile);
        if (!out) {
            cout << "File saving error: " << recordFile << endl;
            return;
        }
        for (const auto& r : records)
            out << r.getId() << "|" << r.getName() << "|"
                << r.getColonialName() << "|" << r.getLatitude() << "|"
                << r.getLongitude() << "|" << static_cast<int>(r.getTier())
                << "|" << r.getOwner() << endl;
    }

    void saveGrants() {
        ofstream out(grantFile);
        if (!out) {
            cout << "File saving error: " << grantFile << endl;
            return;
        }
        for (const auto& g : grants)
            out << g.getId() << "|" << g.getRecordId() << "|" << g.getUser()
                << "|" << g.getStatus() << "|" << g.getExpires() << endl;
    }

    void saveObservations() {
        ofstream out(obsFile);
        if (!out) {
            cout << "File saving error: " << obsFile << endl;
            return;
        }
        for (const auto& o : observations)
            out << o.recordId << "|" << o.type << "|" << o.value << "|"
                << o.date << "|" << o.contributor << endl;
    }

    void loadRecords() {
        ifstream in(recordFile);
        string line;
        while (getline(in, line)) {
            auto d = split(line, '|');
            if (d.size() != 7) continue;
            try {
                records.emplace_back(stoi(d[0]), d[1], d[2], stod(d[3]),
                                     stod(d[4]),
                                     tierFromInt(stoi(d[5])), d[6]);
            } catch (const exception&) {
                cout << "Skipping invalid record entry." << endl;
            }
        }
    }

    void loadGrants() {
        ifstream in(grantFile);
        string line;
        while (getline(in, line)) {
            auto d = split(line, '|');
            if (d.size() != 5) continue;
            try {
                grants.emplace_back(stoi(d[0]), stoi(d[1]), d[2], d[3], d[4]);
            } catch (const exception&) {
                cout << "Skipping invalid grant entry." << endl;
            }
        }
    }

    void loadObservations() {
        ifstream in(obsFile);
        string line;
        while (getline(in, line)) {
            auto d = split(line, '|');
            if (d.size() != 5) continue;
            try {
                observations.push_back(
                    {stoi(d[0]), d[1], stod(d[2]), d[3], d[4]});
            } catch (const exception&) {
                cout << "Skipping invalid observation entry." << endl;
            }
        }
    }

    void addSampleRecords() {
        records.emplace_back(1, "Sample Point", "Long Point", 46.8123, -84.9021,
                             Tier::Open, "Mary");
        records.emplace_back(2, "Cedar Ridge Gathering Area", "Ridge Camp Road",
                             46.7350, -84.8812, Tier::CommunityOnly, "Mary");
        records.emplace_back(3, "Heron Marsh Site", "Marsh Parcel 12", 46.7021,
                             -84.9540, Tier::Restricted, "Mary");
        saveRecords();
    }

public:
    IlnrsService() {
        users = {User("Admin", Role::Admin), User("Mary", Role::Elder),
                 User("Aput", Role::Researcher), User("Guest", Role::Public)};
        loadRecords();
        loadGrants();
        loadObservations();
        if (records.empty()) addSampleRecords();
    }

    const User& login(const string& name) const {
        for (const auto& u : users)
            if (u.getName() == trim(name)) return u;
        throw invalid_argument("User not found.");
    }

    void addRecord(const User& u, const string& name, const string& colonial,
                   double lat, double lon, int tierNumber) {
        if (u.getRole() != Role::Elder)
            throw runtime_error("Only Elders can add place name records.");
        int id = records.empty() ? 1 : records.back().getId() + 1;
        records.emplace_back(id, name, colonial, lat, lon,
                             tierFromInt(tierNumber), u.getName());
        saveRecords();
        log(u, "ADD_RECORD", "Record " + to_string(id));
        cout << "Record added successfully. Record ID: " << id << endl;
    }

    void listRecords(const User& u) {
        int shown = 0;
        for (const auto& r : records)
            if (canView(u, r)) { r.display(); shown++; }
        cout << "Records shown: " << shown << endl;
    }

    void viewRecord(const User& u, int id) {
        PlaceNameRecord& r = findRecord(id);
        if (!canView(u, r)) {
            log(u, "ACCESS_DENIED", "Record " + to_string(id));
            throw runtime_error("Access denied. This record is restricted.");
        }
        r.display();
        int count = 0;
        for (const auto& o : observations)
            if (o.recordId == id) {
                cout << "    Observation   : " << o.type << " = " << o.value
                     << " on " << o.date << " (" << o.contributor << ")"
                     << endl;
                count++;
            }
        if (count == 0) cout << "    Observations  : none recorded" << endl;
    }

    void requestAccess(const User& u, int id) {
        if (u.getRole() != Role::Researcher)
            throw runtime_error("Only Youth Researchers can request access.");
        PlaceNameRecord& r = findRecord(id);
        if (r.getTier() != Tier::Restricted)
            throw invalid_argument(
                "Access requests are only for restricted records.");
        if (canView(u, r))
            throw invalid_argument("You already have access to this record.");
        int gid = grants.empty() ? 1 : grants.back().getId() + 1;
        grants.emplace_back(gid, id, u.getName(), "Requested", "-");
        saveGrants();
        log(u, "REQUEST_ACCESS", "Record " + to_string(id));
        cout << "Request submitted. Request ID: " << gid << endl;
    }

    bool showPending(const User& u) {
        if (u.getRole() != Role::Elder)
            throw runtime_error("Only Elders can review access requests.");
        bool any = false;
        for (const auto& g : grants)
            if (g.getStatus() == "Requested" &&
                findRecord(g.getRecordId()).getOwner() == u.getName()) {
                cout << "Request " << g.getId() << ": " << g.getUser()
                     << " wants record " << g.getRecordId() << endl;
                any = true;
            }
        if (!any) cout << "No pending requests." << endl;
        return any;
    }

    void decide(const User& u, int grantId, bool approve, int days) {
        for (auto& g : grants) {
            if (g.getId() != grantId) continue;
            if (g.getStatus() != "Requested")
                throw invalid_argument("This request is already decided.");
            if (findRecord(g.getRecordId()).getOwner() != u.getName())
                throw runtime_error(
                    "You can decide only on your own records.");
            if (approve) g.approve(days); else g.deny();
            saveGrants();
            log(u, approve ? "GRANT_APPROVED" : "GRANT_DENIED",
                "Request " + to_string(grantId));
            cout << "Request "
                 << (approve ? "approved until " + g.getExpires()
                             : string("denied"))
                 << "." << endl;
            return;
        }
        throw invalid_argument("No request with ID " + to_string(grantId) +
                               ".");
    }

    void addObservation(const User& u, int id, const string& type,
                        double value) {
        if (u.getRole() != Role::Elder && u.getRole() != Role::Researcher)
            throw runtime_error(
                "Only Elders and Researchers can add observations.");
        PlaceNameRecord& r = findRecord(id);
        if (!canView(u, r)) {
            log(u, "ACCESS_DENIED", "Record " + to_string(id));
            throw runtime_error("Access denied. This record is restricted.");
        }
        string t = trim(type);
        checkText(t, "Observation type");
        observations.push_back({id, t, value, stamp(false), u.getName()});
        saveObservations();
        log(u, "ADD_OBSERVATION", "Record " + to_string(id));
        cout << "Observation saved." << endl;
    }

    void search(const User& u, const string& keyword) {
        string k = lower(trim(keyword));
        checkText(k, "Keyword");
        int found = 0;
        for (const auto& r : records)
            if (canView(u, r) &&
                (lower(r.getName()).find(k) != string::npos ||
                 lower(r.getColonialName()).find(k) != string::npos)) {
                r.display();
                found++;
            }
        if (found == 0) cout << "No matching records found." << endl;
    }

    void showAudit(const User& u) {
        if (u.getRole() != Role::Admin)
            throw runtime_error(
                "Only administrators can view the audit log.");
        ifstream in(auditFile);
        string line;
        int n = 0;
        while (getline(in, line)) {
            auto d = split(line, '|');
            if (d.size() != 4) continue;
            cout << ++n << ". " << d[0] << " | " << d[1] << " | " << d[2]
                 << " | " << d[3] << endl;
        }
        if (n == 0) cout << "No audit entries." << endl;
    }
};

}  // namespace ilnrs

using namespace ilnrs;

string readLine(const string& message) {
    string s;
    cout << message;
    getline(cin, s);
    return s;
}

int readInt(const string& message) {
    while (true) {
        try {
            return stoi(trim(readLine(message)));
        } catch (const exception&) {
            cout << "Enter a valid whole number." << endl;
        }
    }
}

int readChoice(const string& message, int min, int max) {
    while (true) {
        int v = readInt(message);
        if (v >= min && v <= max) return v;
        cout << "Enter a number between " << min << " and " << max << "."
             << endl;
    }
}

double readDouble(const string& message) {
    while (true) {
        try {
            return stod(trim(readLine(message)));
        } catch (const exception&) {
            cout << "Enter a valid number." << endl;
        }
    }
}

int main() {
    IlnrsService service;
    cout << "=== INDIGENOUS LAND NAME RECORD SYSTEM ===" << endl;
    const User* current = nullptr;
    for (int attempt = 0; attempt < 3 && current == nullptr; attempt++) {
        try {
            current = &service.login(readLine("Enter your name: "));
        } catch (const invalid_argument& e) {
            cout << "Error: " << e.what() << endl;
        }
    }
    if (current == nullptr) return 1;
    cout << "Welcome, " << current->getName() << " ("
         << roleName(current->getRole()) << ")" << endl;

    int choice;
    do {
        cout << "\n1. Add Place Name Record\n2. List Records\n"
             << "3. View Record by ID\n4. Request Access\n"
             << "5. Review Access Requests\n6. Add Observation\n"
             << "7. Search Records\n8. View Audit Log\n9. Exit" << endl;
        choice = readInt("Enter your choice: ");
        try {
            switch (choice) {
                case 1: {
                    string name = readLine("Enter traditional name: ");
                    string colonial =
                        readLine("Enter colonial name (optional): ");
                    double lat = readDouble("Enter latitude: ");
                    double lon = readDouble("Enter longitude: ");
                    int tier = readChoice(
                        "Access level (1-Public, 2-Community, "
                        "3-Restricted): ", 1, 3);
                    service.addRecord(*current, name, colonial, lat, lon,
                                      tier);
                    break;
                }
                case 2:
                    service.listRecords(*current);
                    break;
                case 3:
                    service.viewRecord(*current,
                                       readInt("Enter record ID: "));
                    break;
                case 4:
                    service.requestAccess(*current,
                                          readInt("Enter record ID: "));
                    break;
                case 5:
                    if (service.showPending(*current)) {
                        int id = readInt("Enter request ID: ");
                        int act = readChoice("1-Approve, 2-Deny: ", 1, 2);
                        int days = 0;
                        if (act == 1)
                            days = readChoice("Days of access (1-30): ", 1, 30);
                        service.decide(*current, id, act == 1, days);
                    }
                    break;
                case 6: {
                    int id = readInt("Enter record ID: ");
                    string type = readLine("Enter observation type: ");
                    double value = readDouble("Enter value: ");
                    service.addObservation(*current, id, type, value);
                    break;
                }
                case 7:
                    service.search(*current, readLine("Enter keyword: "));
                    break;
                case 8:
                    service.showAudit(*current);
                    break;
                case 9:
                    cout << "Thank you for using the system." << endl;
                    break;
                default:
                    cout << "Invalid menu choice." << endl;
            }
        } catch (const exception& e) {
            cout << "Error: " << e.what() << endl;
        }
    } while (choice != 9);
    return 0;
}

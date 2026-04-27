// manual input from the terminal or customer
// ============================================================
//  Smart Pahadi Kitchen Scheduling System
//  Patterns: Factory, Singleton, Strategy | OOP: Polymorphism
//  v3: Real system time | Avg-wait variable window | Fuzzy auto-extend
// ============================================================
#include <iostream>
#include <vector>
#include <string>
#include <memory>
#include <queue>
#include <map>
#include <cmath>
#include <iomanip>
#include <algorithm>
#include <chrono>           // NEW: real-time support
using namespace std;


// ── Real-time helper ────────────────────────────────────────
// Returns current time as fractional minutes since epoch.
// All timestamps in the system use this unit.
double getCurrentTime() {
    using namespace chrono;
    auto now = system_clock::now().time_since_epoch();
    // use double so fractional seconds are preserved
    double secs = duration_cast<duration<double>>(now).count();
    return secs / 60.0;   // convert to minutes
}


enum class ChutneyType { MINT, RADISH, WALNUT, NONE };

struct Customization {
    ChutneyType chutney = ChutneyType::NONE;
    bool addDahi      = false;
    int  spiceLevel   = 3;
    bool extraLarge   = false;

    string toString() const {
        string s = "Spice:" + to_string(spiceLevel);
        if (chutney == ChutneyType::MINT)   s += " +MintChutney";
        if (chutney == ChutneyType::RADISH) s += " +RadishChutney";
        if (chutney == ChutneyType::WALNUT) s += " +WalnutChutney";
        if (addDahi)    s += " +Dahi";
        if (extraLarge) s += " [XL]";
        return s;
    }
};


// ── Menu items ───────────────────────────────────────────────
class MenuItem {
protected:
    string name;
    double basePrepTime;
public:
    MenuItem(const string& n, double t) : name(n), basePrepTime(t) {}
    virtual ~MenuItem() = default;

    virtual double getPrepTime(const Customization& c) const {
        double t = basePrepTime;
        if (c.extraLarge)                        t *= 1.4;
        if (c.chutney != ChutneyType::NONE)      t += 1.5;
        if (c.addDahi)                           t += 2.0;
        return t;
    }
    string getName() const { return name; }
};

class Pakodi      : public MenuItem { public: Pakodi()      : MenuItem("Pahadi Pakodi",        10.0) {} };
class PaneerTikka : public MenuItem { public: PaneerTikka() : MenuItem("Paneer Tikka",          15.0) {} };
class PahadiThali : public MenuItem { public: PahadiThali() : MenuItem("Special Pahadi Thali",  25.0) {} };
class ChickenFry  : public MenuItem { public: ChickenFry()  : MenuItem("Kumaoni Chicken Fry",   20.0) {} };
class JhangoraSoup: public MenuItem { public: JhangoraSoup(): MenuItem("Jhangora Soup",           7.0) {} };
class BuranshJuice: public MenuItem { public: BuranshJuice(): MenuItem("Buransh Juice",           4.0) {} };
class RaiSaag     : public MenuItem { public: RaiSaag()     : MenuItem("Pahadi Rai Saag",        12.0) {} };


// ── Factory ──────────────────────────────────────────────────
class FoodFactory {
public:
    static unique_ptr<MenuItem> createItem(int id) {
        switch (id) {
            case 1: return make_unique<Pakodi>();
            case 2: return make_unique<PaneerTikka>();
            case 3: return make_unique<PahadiThali>();
            case 4: return make_unique<ChickenFry>();
            case 5: return make_unique<JhangoraSoup>();
            case 6: return make_unique<BuranshJuice>();
            case 7: return make_unique<RaiSaag>();
            default: return nullptr;
        }
    }
    static void printMenu() {
        cout << "\n  ID | Item                     | Base Time\n";
        cout << "  ---+--------------------------+----------\n";
        cout << "   1 | Pahadi Pakodi             | 10 min\n";
        cout << "   2 | Paneer Tikka              | 15 min\n";
        cout << "   3 | Special Pahadi Thali      | 25 min\n";
        cout << "   4 | Kumaoni Chicken Fry       | 20 min\n";
        cout << "   5 | Jhangora Soup             |  7 min\n";
        cout << "   6 | Buransh Juice             |  4 min\n";
        cout << "   7 | Pahadi Rai Saag           | 12 min\n";
    }
};


// ── Priority strategy ────────────────────────────────────────
class IPriorityStrategy {
public:
    virtual ~IPriorityStrategy() = default;
    virtual double calculate(double dist, double wait, double prep) const = 0;
};

class SmartSchedulingStrategy : public IPriorityStrategy {
    static constexpr double DIST_W  = 10.0;
    static constexpr double WAIT_W  =  1.5;
    static constexpr double PREP_W  =  0.2;
    static constexpr double WAIT_TH = 20.0;
public:
    double calculate(double dist, double wait, double prep) const override {
        double score = (dist * DIST_W) + (wait * WAIT_W) - (prep * PREP_W);
        if (wait > WAIT_TH) score *= 1.5;
        return score;
    }
};


// ── Order struct ─────────────────────────────────────────────
struct Order {
    int    orderID;
    string customerName;
    string location;
    double distance;
    double waitTime;
    double prepTime;
    double priorityScore;
    string description;
    double timestamp;       // real time in minutes
    int    mergeCount;      // how many individual orders are merged here

    bool operator<(const Order& o) const { return priorityScore < o.priorityScore; }
};


// ── Kitchen Manager (Singleton) ───────────────────────────────
class KitchenManager {
private:
    static KitchenManager* instance;
    priority_queue<Order>  scheduler;
    map<string, Order>     activeBatches;

    // Per-location wait-time tracking for variable window computation.
    // locationWaitSum[loc]   = sum of waitTime values of all orders at loc
    // locationWaitCount[loc] = number of orders contributed to that sum
    // avgWait = sum / count  →  drives the batch window size (16–25 min)
    map<string, double>    locationWaitSum;
    map<string, int>       locationWaitCount;

    unique_ptr<IPriorityStrategy> strategy;
    int orderCounter;
    int chefCount;

    KitchenManager() : orderCounter(0), chefCount(3) {
        strategy = make_unique<SmartSchedulingStrategy>();
    }

    // ── Variable batch window logic (avg-wait-based) ────────
    //
    // The batch window for a location scales with the average wait time
    // of all orders already queued there:
    //
    //   avgWait  = locationWaitSum[loc] / locationWaitCount[loc]
    //
    //   window   = clamp(MIN_WIN + avgWait * WAIT_SCALE, MIN_WIN, MAX_WIN)
    //            = 16 min  when avgWait is very low  (fast, fresh orders)
    //            = 25 min  when avgWait >= ~18 min   (customers already patient)
    //
    // Intuition: if the pending batch already has customers who have been
    // waiting a long time, there is more "slack" to absorb one more order
    // into the same delivery run without making anyone much worse off.
    //
    // Fuzzy auto-extend: if an incoming order misses the computed window
    // by ≤ FUZZY_TOLERANCE minutes, the window is silently extended and
    // the order is merged.  No new batch is created.
    //
    static constexpr double MIN_WIN         = 16.0;  // floor window (min)
    static constexpr double MAX_WIN         = 25.0;  // ceiling window (min)
    static constexpr double WAIT_SCALE      =  0.5;  // window growth per avg-wait min
    static constexpr double FUZZY_TOLERANCE =  2.0;  // ±2 min grace past window

    // Returns the base window (before fuzzy) for a location.
    double computeWindow(const string& location) const {
        auto itS = locationWaitSum.find(location);
        auto itC = locationWaitCount.find(location);
        if (itS == locationWaitSum.end() || itC->second == 0)
            return MIN_WIN;
        double avgWait = itS->second / itC->second;
        double win     = MIN_WIN + avgWait * WAIT_SCALE;
        if (win > MAX_WIN) win = MAX_WIN;
        return win;
    }

    // Returns true when the incoming order should be merged.
    // Also sets isFuzzy=true when merge is only possible via the grace zone.
    bool withinBatchWindow(const string& location, double timeNow,
                           double existingTimestamp, bool& isFuzzy) const {
        double delta   = fabs(existingTimestamp - timeNow);
        double baseWin = computeWindow(location);
        if (delta <= baseWin) { isFuzzy = false; return true;  }
        if (delta <= baseWin + FUZZY_TOLERANCE) { isFuzzy = true; return true; }
        return false;
    }

public:
    static KitchenManager* getInstance() {
        if (!instance) instance = new KitchenManager();
        return instance;
    }

    void setChefCount(int n) { chefCount = n; }

    void placeOrder(const string& customerName,
                    const string& location,
                    double dist, double wait,
                    int foodID, const Customization& cust) {

        // Use REAL current time (minutes)
        double timeNow = getCurrentTime();

        auto item = FoodFactory::createItem(foodID);
        if (!item) { cout << "  [!] Invalid food ID: " << foodID << "\n"; return; }

        double cookTime = item->getPrepTime(cust);
        string desc     = item->getName() + " [" + cust.toString() + "]";

        map<string, Order>::iterator it = activeBatches.find(location);
        bool isFuzzy = false;

        if (it != activeBatches.end() &&
            withinBatchWindow(location, timeNow, it->second.timestamp, isFuzzy)) {

            // ── Merge into existing batch ─────────────────────
            Order& existing = it->second;
            existing.prepTime    += cookTime;
            existing.description += " + " + desc;
            existing.mergeCount  += 1;
            existing.priorityScore = strategy->calculate(
                existing.distance, existing.waitTime, existing.prepTime);

            // Update avg-wait accumulators with the new order's wait time
            locationWaitSum[location]   += wait;
            locationWaitCount[location] += 1;

            double newWindow = computeWindow(location);  // recomputed after update

            if (isFuzzy) {
                double delta = fabs(it->second.timestamp - timeNow);
                cout << "  [FUZZY-BATCH] Window auto-extended for '" << location
                     << "' (delta=" << fixed << setprecision(2) << delta
                     << " min, base-window=" << setprecision(1) << (newWindow)
                     << " min). Merged '" << customerName << "'.\n";
            } else {
                cout << "  [BATCH]  Merged '" << customerName
                     << "' into batch for " << location
                     << " (avg-wait=" << fixed << setprecision(1)
                     << (locationWaitSum[location] / locationWaitCount[location])
                     << " min → window=" << newWindow << " min)\n";
            }

        } else {
            // ── New batch ─────────────────────────────────────
            ++orderCounter;
            double score = strategy->calculate(dist, wait, cookTime);
            Order newOrder = { orderCounter, customerName, location,
                               dist, wait, cookTime, score, desc, timeNow, 1 };
            activeBatches[location]      = newOrder;
            locationWaitSum[location]    = wait;
            locationWaitCount[location]  = 1;

            cout << "  [ORDER]  #" << orderCounter << " | " << customerName
                 << " @ " << location << " | " << item->getName()
                 << " | initial-window=" << fixed << setprecision(1)
                 << computeWindow(location) << " min\n";
        }
    }

    void processQueue() {
        for (map<string, Order>::iterator it = activeBatches.begin();
             it != activeBatches.end(); ++it) {
            scheduler.push(it->second);
        }

        const int W = 115;
        cout << "\n" << string(W, '=') << "\n";
        cout << "         SMART PAHADI KITCHEN -- DISPATCH ORDER (Highest Priority First)\n";
        cout << string(W, '=') << "\n";
        cout << left
             << setw(5)  << "Chef"
             << setw(5)  << "ID"
             << setw(20) << "Customer"
             << setw(18) << "Location"
             << setw(6)  << "Dist"
             << setw(7)  << "Wait"
             << setw(7)  << "Prep"
             << setw(8)  << "Score"
             << setw(7)  << "Merged"
             << "Items\n";
        cout << string(W, '-') << "\n";

        int chef = 1;
        while (!scheduler.empty()) {
            Order o = scheduler.top(); scheduler.pop();

            string desc = o.description.length() > 35
                        ? o.description.substr(0, 32) + "..."
                        : o.description;

            cout << left
                 << setw(5)  << ("C" + to_string(chef))
                 << setw(5)  << o.orderID
                 << setw(20) << o.customerName
                 << setw(18) << o.location
                 << setw(6)  << fixed << setprecision(1) << o.distance
                 << setw(7)  << o.waitTime
                 << setw(7)  << o.prepTime
                 << setw(8)  << setprecision(2) << o.priorityScore
                 << setw(7)  << o.mergeCount
                 << desc << "\n";

            chef = (chef % chefCount) + 1;
        }
        cout << string(W, '=') << "\n";
    }

    void showBatchingSummary() const {
        cout << "\n--- Delivery Batching Summary ---\n";
        bool anyBatch = false;
        for (map<string, Order>::const_iterator it = activeBatches.begin();
             it != activeBatches.end(); ++it) {
            const Order& o = it->second;
            if (o.mergeCount > 1) {
                double win = computeWindow(o.location);
                // Compute avg wait for display
                auto itS = locationWaitSum.find(o.location);
                auto itC = locationWaitCount.find(o.location);
                double avgW = (itS != locationWaitSum.end() && itC->second > 0)
                              ? itS->second / itC->second : 0.0;
                cout << "  [Batch Delivery] Order #" << o.orderID
                     << " -> " << o.location
                     << " | " << o.mergeCount << " orders merged"
                     << " | avg-wait=" << fixed << setprecision(1) << avgW
                     << " min → window=" << win << " min\n";
                anyBatch = true;
            }
        }
        if (!anyBatch) cout << "  No batching opportunities found.\n";
    }
};

KitchenManager* KitchenManager::instance = nullptr;


// ── Input helper ─────────────────────────────────────────────
static void flushAndGetLine(string& out) {
    if (cin.peek() == '\n') cin.ignore();
    getline(cin, out);
}


// ── Main ─────────────────────────────────────────────────────
int main() {
    KitchenManager* kitchen = KitchenManager::getInstance();
    kitchen->setChefCount(3);

    cout << "\n";
    cout << "  +----------------------------------------------+\n";
    cout << "  |      Smart Pahadi Kitchen System  v3         |\n";
    cout << "  |   Real-time | Avg-Wait Variable Window       |\n";
    cout << "  +----------------------------------------------+\n";

    FoodFactory::printMenu();

    // ----------------------------------------------------------
    // SECTION A: Pre-loaded simulation orders
    // NOTE: placeOrder no longer takes a 'timeNow' argument —
    //       it reads the real system clock internally.
    //       A small sleep between orders ensures distinct timestamps
    //       so the variable window logic is visible in output.
    // ----------------------------------------------------------
    cout << "\n--- Pre-loaded Simulation Orders ---\n";

    Customization c1; c1.addDahi = true;  c1.spiceLevel = 2;
    kitchen->placeOrder("Peehu Gupta",  "Saraswati H. R-110", 0.6,  5.0, 1, c1);

    Customization c2; c2.chutney = ChutneyType::MINT; c2.addDahi = true; c2.spiceLevel = 4;
    kitchen->placeOrder("Yuvraj Gupta", "Alaknanda A-215",    1.2,  2.0, 4, c2);

    Customization c3; c3.spiceLevel = 3;
    kitchen->placeOrder("Ayush Bharti", "Alaknanda A-215",    1.2,  8.0, 3, c3);
    // ^ Same location as Yuvraj — placed within seconds → will merge (window starts at 16 min)

    Customization c4; c4.extraLarge = true; c4.spiceLevel = 5;
    kitchen->placeOrder("Admin Office", "Main Gate",          4.0,  1.0, 2, c4);

    Customization c5; c5.chutney = ChutneyType::WALNUT; c5.spiceLevel = 3;
    kitchen->placeOrder("Riya Sharma",  "Hostel-A Room-1",    0.8,  5.0, 5, c5);

    Customization c6; c6.spiceLevel = 1;
    kitchen->placeOrder("Karan Mehta",  "Hostel-A Room-1",    0.8,  5.0, 6, c6);
    // ^ Same location as Riya — placed within seconds → will merge

    Customization c7; c7.chutney = ChutneyType::RADISH; c7.spiceLevel = 4;
    kitchen->placeOrder("Prof. Tiwari", "Faculty Block-2",    2.0, 28.0, 7, c7);

    Customization c8; c8.spiceLevel = 2;
    kitchen->placeOrder("Simran Negi",  "Central Library",    1.5, 10.0, 1, c8);

    // ----------------------------------------------------------
    // SECTION B: Live terminal input
    // ----------------------------------------------------------
    cout << "\n+--------------------------------------------------+\n";
    cout << "|         LIVE ORDER ENTRY  (Your Input)           |\n";
    cout << "+--------------------------------------------------+\n";
    cout << "  How many custom orders to add? (0 to skip): ";
    int n; cin >> n;

    for (int i = 0; i < n; i++) {
        cout << "\n  --- Custom Order " << (i + 1) << " of " << n << " ---\n";

        string custName, address;
        double dist, waitMin;
        int    foodID;

        cout << "  Customer name         : ";
        flushAndGetLine(custName);

        cout << "  Address / location    : ";
        flushAndGetLine(address);

        cout << "  Distance in km        : ";
        cin >> dist;

        cout << "  Already waited (min)  : ";
        cin >> waitMin;

        FoodFactory::printMenu();
        cout << "  Dish ID (1-7)         : ";
        cin >> foodID;

        Customization cust;

        cout << "  Spice level (1-5)     : ";
        cin >> cust.spiceLevel;
        if (cust.spiceLevel < 1) cust.spiceLevel = 1;
        if (cust.spiceLevel > 5) cust.spiceLevel = 5;

        cout << "  Add Dahi?  (1=Yes 0=No): ";
        int dahi; cin >> dahi; cust.addDahi = (dahi == 1);

        cout << "  Extra Large? (1=Yes 0=No): ";
        int xl; cin >> xl; cust.extraLarge = (xl == 1);

        cout << "  Chutney (0=None 1=Mint 2=Radish 3=Walnut): ";
        int ch; cin >> ch;
        if      (ch == 1) cust.chutney = ChutneyType::MINT;
        else if (ch == 2) cust.chutney = ChutneyType::RADISH;
        else if (ch == 3) cust.chutney = ChutneyType::WALNUT;
        else              cust.chutney = ChutneyType::NONE;

        // Real clock is used automatically inside placeOrder
        kitchen->placeOrder(custName, address, dist, waitMin, foodID, cust);
    }

    // ----------------------------------------------------------
    // SECTION C: Process all orders together and display
    // ----------------------------------------------------------
    kitchen->processQueue();
    kitchen->showBatchingSummary();

    cout << "\n  [OK] All orders dispatched via Smart Scheduling Strategy.\n\n";
    return 0;
}

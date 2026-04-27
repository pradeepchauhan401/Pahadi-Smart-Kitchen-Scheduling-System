// system with predefined input ready 

// ============================================================
//  Smart Pahadi Kitchen Scheduling System
//  Patterns: Factory, Singleton, Strategy | OOP: Polymorphism
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
using namespace std;

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
class JhangoraSoup: public MenuItem { public: JhangoraSoup(): MenuItem("Jhangora Soup",          7.0) {} };
class BuranshJuice: public MenuItem { public: BuranshJuice(): MenuItem("Buransh Juice",          4.0) {} };
class RaiSaag     : public MenuItem { public: RaiSaag()     : MenuItem("Pahadi Rai Saag",       12.0) {} };


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


class IPriorityStrategy {
public:
    virtual ~IPriorityStrategy() = default;
    virtual double calculate(double dist, double wait, double prep) const = 0;
};

// Score = distance-weighted + wait-weighted – prep penalty
// Anti-starvation: 50 % boost when wait > 20 min
class SmartSchedulingStrategy : public IPriorityStrategy {
    static constexpr double DIST_W  = 10.0;
    static constexpr double WAIT_W  =  1.5;
    static constexpr double PREP_W  =  0.2;
    static constexpr double WAIT_TH = 20.0;
public:
    double calculate(double dist, double wait, double prep) const override {
        double score = (dist * DIST_W) + (wait * WAIT_W) - (prep * PREP_W);
        if (wait > WAIT_TH) score *= 1.5;   // Fairness / anti-starvation
        return score;
    }
};

// ============================================================
// 5. ORDER ENTITY
// ============================================================
struct Order {
    int    orderID;
    string customerName;
    string location;
    double distance;    // km
    double waitTime;    // minutes already waited
    double prepTime;    // minutes to prepare
    double priorityScore;
    string description;
    double timestamp;   // simulated minutes since system start

    bool operator<(const Order& o) const { return priorityScore < o.priorityScore; }
};



class KitchenManager {
private:
    static KitchenManager* instance;
    priority_queue<Order>  scheduler;
    map<string, Order>     activeBatches;   // keyed by location
    unique_ptr<IPriorityStrategy> strategy;
    int  orderCounter;
    int  chefCount;

    KitchenManager() : orderCounter(0), chefCount(3) {
        strategy = make_unique<SmartSchedulingStrategy>();
    }

public:
    static KitchenManager* getInstance() {
        if (!instance) instance = new KitchenManager();
        return instance;
    }

    void setChefCount(int n) { chefCount = n; }

    // Temporal batching window: 30 min
    static constexpr double BATCH_WINDOW = 30.0;

    void placeOrder(const string& customerName,
                    const string& location,
                    double dist, double wait, double timeNow,
                    int foodID, const Customization& cust) {

        auto item = FoodFactory::createItem(foodID);
        if (!item) { cout << "  [!] Invalid food ID: " << foodID << "\n"; return; }

        double cookTime = item->getPrepTime(cust);
        string desc     = item->getName() + " [" + cust.toString() + "]";

        // Check if an open batch exists for this location within the time window
        map<string, Order>::iterator it = activeBatches.find(location);
        if (it != activeBatches.end() &&
            fabs(it->second.timestamp - timeNow) <= BATCH_WINDOW) {

            Order& existing = it->second;
            existing.prepTime    += cookTime;
            existing.description += " + " + desc;
            existing.priorityScore = strategy->calculate(
                existing.distance, existing.waitTime, existing.prepTime);

            cout << "  [BATCH]  Merged '" << customerName
                 << "' into existing order for " << location << "\n";
        } else {
            ++orderCounter;
            double score = strategy->calculate(dist, wait, cookTime);
            Order newOrder = { orderCounter, customerName, location,
                               dist, wait, cookTime, score, desc, timeNow };
            activeBatches[location] = newOrder;
            cout << "  [ORDER]  #" << orderCounter << " | " << customerName
                 << " @ " << location << " | " << item->getName() << "\n";
        }
    }

    void processQueue() {
        // Push all batched orders into priority queue
        for (map<string, Order>::iterator it = activeBatches.begin();
             it != activeBatches.end(); ++it) {
            scheduler.push(it->second);
        }

        const int W = 110;
        cout << "\n" << string(W, '=') << "\n";
        cout << "         SMART PAHADI KITCHEN — DISPATCH ORDER (Highest Priority First)\n";
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
             << "Items\n";
        cout << string(W, '-') << "\n";

        int chef = 1;
        while (!scheduler.empty()) {
            Order o = scheduler.top(); scheduler.pop();

            string desc = o.description.length() > 38
                        ? o.description.substr(0, 35) + "..."
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
                 << desc << "\n";

            chef = (chef % chefCount) + 1;
        }
        cout << string(W, '=') << "\n";
    }

    void showBatchingSummary() const {
        cout << "\n--- Delivery Batching Summary ---\n";
        vector<Order> orders;
        for (map<string, Order>::const_iterator it = activeBatches.begin();
             it != activeBatches.end(); ++it) {
            orders.push_back(it->second);
        }
        bool anyBatch = false;
        for (size_t i = 0; i < orders.size(); ++i) {
            // A batched order has " + " in its description
            if (orders[i].description.find(" + ") != string::npos) {
                cout << "  [Batch Delivery] Order #" << orders[i].orderID
                     << " → " << orders[i].location
                     << " (merged items)\n";
                anyBatch = true;
            }
        }
        if (!anyBatch) cout << "  No batching opportunities found.\n";
    }
};

KitchenManager* KitchenManager::instance = nullptr;


int main() {
    KitchenManager* kitchen = KitchenManager::getInstance();
    kitchen->setChefCount(3);

    cout << "\n";
    cout << "  ╔══════════════════════════════════════════════╗\n";
    cout << "  ║   🏔  Smart Pahadi Kitchen System  🏔       ║\n";
    cout << "  ╚══════════════════════════════════════════════╝\n";

    FoodFactory::printMenu();
    cout << "\n--- Incoming Orders ---\n";

    // ── Peehu Gupta ──────────────────────────────────
    Customization c1;
    c1.addDahi    = true;
    c1.spiceLevel = 2;
    kitchen->placeOrder("Peehu Gupta", "Saraswati H. R-110 ", 0.6, 5.0, 10.0, 1, c1);

    // ── Yuvraj Gupta ─────────────────────────────────
    Customization c2;
    c2.chutney    = ChutneyType::MINT;
    c2.addDahi    = true;
    c2.spiceLevel = 4;
    kitchen->placeOrder("Yuvraj Gupta", "Alaknanda A-215", 1.2, 2.0, 15.0, 4, c2);

    // ── Ayush Bharti  (nearby Yuvraj — tests batching) ─
    Customization c3;
    c3.spiceLevel = 3;
    kitchen->placeOrder("Ayush Bharti", "Alaknanda A-215", 1.2, 8.0, 20.0, 3, c3);

    // ── Admin Block (far → high priority) ────────────
    Customization c4;
    c4.extraLarge = true;
    c4.spiceLevel = 5;
    kitchen->placeOrder("Admin Office", "Main Gate", 4.0, 1.0, 25.0, 2, c4);

    // ── Hostel A Room 1 ───────────────────────────────
    Customization c5;
    c5.chutney    = ChutneyType::WALNUT;
    c5.spiceLevel = 3;
    kitchen->placeOrder("Riya Sharma",  "Hostel-A Room-1", 0.8, 5.0, 10.0, 5, c5);

    // ── Hostel A Room 1 again (batch window) ─────────
    Customization c6;
    c6.spiceLevel = 1;
    kitchen->placeOrder("Karan Mehta",  "Hostel-A Room-1", 0.8, 5.0, 25.0, 6, c6);

    // ── Long-waiting order (anti-starvation boost) ───
    Customization c7;
    c7.chutney    = ChutneyType::RADISH;
    c7.spiceLevel = 4;
    kitchen->placeOrder("Prof. P.Tiwari", "Faculty Block-2", 2.0, 28.0, 30.0, 7, c7);

    // ── Library ──────────────────────────────────────
    Customization c8;
    c8.spiceLevel = 2;
    kitchen->placeOrder("Simran Negi",  "Central Library", 1.5, 10.0, 35.0, 1, c8);

    // ── Process & display ─────────────────────────────
    kitchen->processQueue();
    kitchen->showBatchingSummary();

    cout << "\n  [✓] All orders dispatched via Smart Scheduling Strategy.\n\n";
    return 0;
}

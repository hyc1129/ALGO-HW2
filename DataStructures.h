#ifndef DATA_STRUCTURES_H
#define DATA_STRUCTURES_H

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <iomanip>
#include <iostream>
#include <limits>
#include <queue>
#include <random>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

namespace algo_hw2 {

/**
 * @brief Lightweight global allocation counter to verify zero memory leaks
 *        across all dynamically allocated data structure nodes.
 * @brief Tracks live dynamically allocated nodes across all four structures
 *        to verify 100% deallocation (zero memory leaks) in correctness tests.
 */
struct MemoryTracker {
    static std::size_t& LiveNodes() noexcept {
        static std::size_t live_nodes = 0;
        return live_nodes;
    }

    static std::size_t& TotalAllocations() noexcept {
        static std::size_t total_allocs = 0;
        return total_allocs;
    }

    static std::size_t& TotalDeallocations() noexcept {
        static std::size_t total_deallocs = 0;
        return total_deallocs;
    }

    static inline void OnAllocate() noexcept {
        ++LiveNodes();
        ++TotalAllocations();
    }

    static inline void OnDeallocate() noexcept {
        --LiveNodes();
        ++TotalDeallocations();
    }

    static inline void ResetCounters() noexcept {
        LiveNodes() = 0;
        TotalAllocations() = 0;
        TotalDeallocations() = 0;
    }
};

// ============================================================================
// 1. Unbalanced Binary Search Tree (BST)
// ============================================================================

class BST {
private:
    struct Node {
        int id;
        long long sum_score;
        int count;
        Node* left;
        Node* right;

        Node(int node_id, int score)
            : id(node_id),
              sum_score(score),
              count(1),
              left(nullptr),
              right(nullptr) {
            MemoryTracker::OnAllocate();
        }

        ~Node() {
            MemoryTracker::OnDeallocate();
        }
    };

    Node* root_;

    /**
     * @brief Iterative post-order fallback for pathologically deep trees
     *        (e.g., > 4096 levels on sorted input) to prevent stack overflow
     *        while strictly preserving left -> right -> node deletion order.
     * @brief Iterative post-order fallback for skewed trees (e.g., sorted IDs
     *        1..2^10 or deeper) to prevent call-stack overflow while strictly
     *        preserving Left -> Right -> Root deletion order.
     */
    static void DestroyPostOrderIterative(Node* subtree_root) {
        if (subtree_root == nullptr) return;
        std::vector<Node*> stack;
        stack.push_back(subtree_root);
        Node* last_deleted = nullptr;

        while (!stack.empty()) {
            Node* curr = stack.back();
            // Leaf node, or both children have already been visited and deleted
            bool left_done = (curr->left == nullptr);
            bool right_done = (curr->right == nullptr);
            if (curr->right != nullptr && curr->right == last_deleted) {
                curr->right = nullptr;
                right_done = true;
                left_done = true;  // Right is visited after left in post-order
                left_done = true;
            } else if (curr->left != nullptr && curr->left == last_deleted) {
                curr->left = nullptr;
                left_done = true;
            }

            if (!left_done) {
                stack.push_back(curr->left);
            } else if (!right_done) {
                stack.push_back(curr->right);
            } else {
                stack.pop_back();
                last_deleted = curr;
                delete curr;  // Post-order: deleted strictly after left & right
                delete curr;
            }
        }
    }

    /**
     * @brief Strictly frees all nodes using post-order traversal (Left, Right, Root).
     */
    static void DestroyPostOrder(Node* node, int depth = 0) {
    static void DestroyPostOrderBST(Node* node, int depth = 0) {
        if (node == nullptr) return;
        if (depth > 4096) {
            DestroyPostOrderIterative(node);
            return;
        }
        DestroyPostOrder(node->left, depth + 1);
        DestroyPostOrder(node->right, depth + 1);
        DestroyPostOrderBST(node->left, depth + 1);
        DestroyPostOrderBST(node->right, depth + 1);
        delete node;
    }

    static int ComputeHeightBFS(const Node* start) {
        if (start == nullptr) return 0;
        std::queue<const Node*> q;
        q.push(start);
        int levels = 0;
        while (!q.empty()) {
            std::size_t level_size = q.size();
            ++levels;
            for (std::size_t i = 0; i < level_size; ++i) {
                const Node* curr = q.front();
                q.pop();
                if (curr->left != nullptr) q.push(curr->left);
                if (curr->right != nullptr) q.push(curr->right);
            }
        }
        return levels;
    }

    static int ComputeHeightRec(const Node* node, int depth) {
        if (node == nullptr) return 0;
        if (depth > 4096) {
            return ComputeHeightBFS(node);
        }
        return 1 + std::max(ComputeHeightRec(node->left, depth + 1),
                            ComputeHeightRec(node->right, depth + 1));
    }

    static bool VerifyRec(const Node* node, long long min_id, long long max_id) {
        if (node == nullptr) return true;
        if (node->id <= min_id || node->id >= max_id || node->count <= 0) {
            return false;
        }
        return VerifyRec(node->left, min_id, node->id) &&
               VerifyRec(node->right, node->id, max_id);
    }

public:
    BST() : root_(nullptr) {}

    // Prohibit shallow copies (Rule of Three/Five) to prevent double-free
    // CreateBST OOP equivalent: constructs a BST containing a single initial record
    BST(int id, int score) : root_(new Node(id, score)) {}

    BST(const BST&) = delete;
    BST& operator=(const BST&) = delete;

    /**
     * @brief Destructor strictly freeing all nodes in post-order.
     */
    ~BST() {
        DestroyPostOrder(root_);
        DestroyPostOrderBST(root_);
        root_ = nullptr;
    }

    /**
     * @brief Inserts (id, score) using the "one node per ID" rule.
     *        Duplicate IDs accumulate sum_score and increment count.
     * @brief Inserts (id, score) into the BST without rotations.
     *        Duplicate IDs accumulate sum_score and count in the existing node.
     */
    void Insert(int id, int score) {
    void InsertBST(int id, int score) {
        if (root_ == nullptr) {
            root_ = new Node(id, score);
            return;
        }

        Node* curr = root_;
        Node* parent = nullptr;
        while (curr != nullptr) {
            parent = curr;
            if (id == curr->id) {
                curr->sum_score += score;
                curr->count += 1;
                return;
            } else if (id < curr->id) {
                curr = curr->left;
            } else {
                curr = curr->right;
            }
        }

        if (id < parent->id) {
            parent->left = new Node(id, score);
        } else {
            parent->right = new Node(id, score);
        }
    }

    /**
     * @brief Searches for id and returns its average score, or -1.0 if not found.
     * @brief Returns the average score of `id`, or -1.0 if `id` was never inserted.
     */
    double SearchAVG(int id) const {
    double SearchAVGBST(int id) const {
        const Node* curr = root_;
        while (curr != nullptr) {
            if (id == curr->id) {
                return static_cast<double>(curr->sum_score) / curr->count;
            } else if (id < curr->id) {
                curr = curr->left;
            } else {
                curr = curr->right;
            }
        }
        return -1.0;
    }

    /**
     * @brief Returns the number of levels in the tree (0 if empty).
     * @brief Returns the number of levels in the BST (empty = 0, root-only = 1).
     */
    int Height() const {
    int HeightBST() const {
        return ComputeHeightRec(root_, 0);
    }

    /**
     * @brief Prints the tree level-by-level with parent/direction annotations.
     * @brief Prints all BST nodes level-by-level, explicitly showing parent & L/R side.
     */
    void Print() const {
        int h = Height();
        std::cout << "[BST] Total Levels (Height) = " << h << "\n";
    void PrintBST() const {
        int h = HeightBST();
        std::cout << "[BST] Height = " << h << "\n";
        if (root_ == nullptr) {
            std::cout << "  (empty)\n";
            return;
        }

        struct QueueItem {
            const Node* node;
            int parent_id;
            char side;  // 'O' = root, 'L' = left child, 'R' = right child
            char side;
        };

        std::queue<QueueItem> q;
        q.push(QueueItem{root_, -1, 'O'});
        int level = 1;

        while (!q.empty()) {
            std::size_t sz = q.size();
            std::cout << "  Level " << level << ": ";
            for (std::size_t i = 0; i < sz; ++i) {
                QueueItem item = q.front();
                q.pop();
                const Node* u = item.node;
                double avg = static_cast<double>(u->sum_score) / u->count;

                std::cout << "[id=" << u->id
                          << ", avg=" << std::fixed << std::setprecision(2) << avg
                          << " (sum=" << u->sum_score << ", cnt=" << u->count << "), ";
                if (item.side == 'O') {
                    std::cout << "root]";
                } else {
                    std::cout << item.side << " of " << item.parent_id << "]";
                }
                if (i + 1 < sz) std::cout << " ";

                if (u->left != nullptr) q.push(QueueItem{u->left, u->id, 'L'});
                if (u->right != nullptr) q.push(QueueItem{u->right, u->id, 'R'});
            }
            std::cout << "\n";
            ++level;
        }
    }

    /**
     * @brief Validates BST ordering invariants (used in --test mode).
     */
    bool Verify() const {
    bool VerifyBST() const {
        return VerifyRec(root_,
                         std::numeric_limits<long long>::min(),
                         std::numeric_limits<long long>::max());
    }

    // Generic OOP method aliases
    inline void Insert(int id, int score) { InsertBST(id, score); }
    inline double SearchAVG(int id) const { return SearchAVGBST(id); }
    inline int Height() const { return HeightBST(); }
    inline void Print() const { PrintBST(); }
    inline bool Verify() const { return VerifyBST(); }
};

// ============================================================================
// 2. Parameterized AVL Tree (Height-Balance Limit = delta)
// 2. Parameterized AVL Tree (Height-Difference Limit = delta)
// ============================================================================

class AVL {
private:
    struct Node {
        int id;
        long long sum_score;
        int count;
        int height;  // 1-based level height (leaf = 1, nullptr = 0)
        Node* left;
        Node* right;

        Node(int node_id, int score)
            : id(node_id),
              sum_score(score),
              count(1),
              height(1),
              left(nullptr),
              right(nullptr) {
            MemoryTracker::OnAllocate();
        }

        ~Node() {
            MemoryTracker::OnDeallocate();
        }
    };

    Node* root_;
    int delta_;
    std::uint64_t rotation_count_;  // Single rotation = +1, Double rotation = +2

    static inline int NodeHeight(const Node* node) noexcept {
        return (node != nullptr) ? node->height : 0;
    }

    static inline int BalanceFactor(const Node* node) noexcept {
        return (node != nullptr) ? (NodeHeight(node->left) - NodeHeight(node->right)) : 0;
    }

    static inline void UpdateHeight(Node* node) noexcept {
        int lh = NodeHeight(node->left);
        int rh = NodeHeight(node->right);
        node->height = 1 + ((lh > rh) ? lh : rh);
    }

    /**
     * @brief Right rotation around unbalanced pivot y:
     *          y                x
     *         / \              / \
     *        x   T3    -->    T1  y
     *       / \                  / \
     *      T1  T2               T2  T3
     */
    static Node* RotateRight(Node* y) noexcept {
    Node* RotateRightAVL(Node* y) noexcept {
        Node* x = y->left;
        Node* t2 = x->right;

        x->right = y;
        y->left = t2;

        UpdateHeight(y);
        UpdateHeight(x);
        ++rotation_count_;
        return x;
    }

    /**
     * @brief Left rotation around unbalanced pivot x:
     *        x                    y
     *       / \                  / \
     *      T1  y       -->      x   T3
     *         / \              / \
     *        T2  T3           T1  T2
     */
    static Node* RotateLeft(Node* x) noexcept {
    Node* RotateLeftAVL(Node* x) noexcept {
        Node* y = x->right;
        Node* t2 = y->left;

        y->left = x;
        x->right = t2;

        UpdateHeight(x);
        UpdateHeight(y);
        ++rotation_count_;
        return y;
    }

    Node* InsertRec(Node* node, int id, int score) {
    Node* InsertRecAVL(Node* node, int id, int score) {
        if (node == nullptr) {
            return new Node(id, score);
        }

        if (id == node->id) {
            node->sum_score += score;
            node->count += 1;
            return node;  // Structure unchanged; no height change or rotation
            return node;
        } else if (id < node->id) {
            node->left = InsertRec(node->left, id, score);
            node->left = InsertRecAVL(node->left, id, score);
        } else {
            node->right = InsertRec(node->right, id, score);
            node->right = InsertRecAVL(node->right, id, score);
        }

        UpdateHeight(node);
        int bf = BalanceFactor(node);

        // Generalized delta-AVL rebalancing:
        // Left-heavy violation: height(left) - height(right) > delta_
        // === [REPORT HIGHLIGHT: AVL delta check & single/double rotation] ===
        int bf = BalanceFactor(node);  // height(left) - height(right)
        if (bf > delta_) {
            // Left subtree exceeds right subtree by more than delta_
            if (BalanceFactor(node->left) < 0) {
                // Left-Right (LR) case
                node->left = RotateLeft(node->left);
                node->left = RotateLeftAVL(node->left);   // Left-Right (double rotation)
            }
            // Left-Left (LL) case
            return RotateRight(node);
            return RotateRightAVL(node);                  // Left-Left (single rotation)
        }

        // Right-heavy violation: height(left) - height(right) < -delta_
        if (bf < -delta_) {
            // Right subtree exceeds left subtree by more than delta_
            if (BalanceFactor(node->right) > 0) {
                // Right-Left (RL) case
                node->right = RotateRight(node->right);
                node->right = RotateRightAVL(node->right); // Right-Left (double rotation)
            }
            // Right-Right (RR) case
            return RotateLeft(node);
            return RotateLeftAVL(node);                    // Right-Right (single rotation)
        }
        // === [END REPORT HIGHLIGHT] ===

        return node;
    }

    /**
     * @brief Strictly frees all nodes using post-order traversal (Left, Right, Root).
     */
    static void DestroyPostOrder(Node* node) {
    static void DestroyPostOrderAVL(Node* node) {
        if (node == nullptr) return;
        DestroyPostOrder(node->left);
        DestroyPostOrder(node->right);
        DestroyPostOrderAVL(node->left);
        DestroyPostOrderAVL(node->right);
        delete node;
    }

    bool VerifyRec(const Node* node, long long min_id, long long max_id) const {
        if (node == nullptr) return true;
        if (node->id <= min_id || node->id >= max_id || node->count <= 0) {
            return false;
        }
        int expected_h = 1 + std::max(NodeHeight(node->left), NodeHeight(node->right));
        if (node->height != expected_h) {
            return false;
        }
        if (std::abs(BalanceFactor(node)) > delta_) {
            return false;
        }
        return VerifyRec(node->left, min_id, node->id) &&
               VerifyRec(node->right, node->id, max_id);
    }

public:
    explicit AVL(int delta = 1) : root_(nullptr), delta_(delta) {
    explicit AVL(int delta = 1)
        : root_(nullptr), delta_(delta), rotation_count_(0) {
        if (delta < 1) {
            throw std::invalid_argument("AVL height difference limit delta must be >= 1.");
            throw std::invalid_argument("AVL delta must be >= 1.");
        }
    }

    // CreateAVL OOP equivalent: constructs an AVL tree with one initial record
    AVL(int id, int score, int delta = 1)
        : root_(nullptr), delta_(delta), rotation_count_(0) {
        if (delta < 1) {
            throw std::invalid_argument("AVL delta must be >= 1.");
        }
        root_ = new Node(id, score);
    }

    AVL(const AVL&) = delete;
    AVL& operator=(const AVL&) = delete;

    /**
     * @brief Destructor strictly freeing all nodes in post-order.
     */
    ~AVL() {
        DestroyPostOrder(root_);
        DestroyPostOrderAVL(root_);
        root_ = nullptr;
    }

    int GetDelta() const noexcept { return delta_; }
    std::uint64_t GetRotationCount() const noexcept { return rotation_count_; }
    void ResetRotationCount() noexcept { rotation_count_ = 0; }

    void Insert(int id, int score) {
        root_ = InsertRec(root_, id, score);
    void InsertAVL(int id, int score) {
        root_ = InsertRecAVL(root_, id, score);
    }

    double SearchAVG(int id) const {
    double SearchAVGAVL(int id) const {
        const Node* curr = root_;
        while (curr != nullptr) {
            if (id == curr->id) {
                return static_cast<double>(curr->sum_score) / curr->count;
            } else if (id < curr->id) {
                curr = curr->left;
            } else {
                curr = curr->right;
            }
        }
        return -1.0;
    }

    /**
     * @brief Returns the number of levels in the AVL tree (0 if empty).
     */
    int Height() const noexcept {
    int HeightAVL() const noexcept {
        return NodeHeight(root_);
    }

    void Print() const {
        std::cout << "[AVL (delta=" << delta_ << ")] Total Levels (Height) = "
                  << Height() << "\n";
    void PrintAVL() const {
        std::cout << "[AVL (delta=" << delta_ << ")] Height = " << HeightAVL() << "\n";
        if (root_ == nullptr) {
            std::cout << "  (empty)\n";
            return;
        }

        struct QueueItem {
            const Node* node;
            int parent_id;
            char side;
        };

        std::queue<QueueItem> q;
        q.push(QueueItem{root_, -1, 'O'});
        int level = 1;

        while (!q.empty()) {
            std::size_t sz = q.size();
            std::cout << "  Level " << level << ": ";
            for (std::size_t i = 0; i < sz; ++i) {
                QueueItem item = q.front();
                q.pop();
                const Node* u = item.node;
                double avg = static_cast<double>(u->sum_score) / u->count;

                std::cout << "[id=" << u->id
                          << ", avg=" << std::fixed << std::setprecision(2) << avg
                          << " (cnt=" << u->count << ")"
                          << ", h=" << u->height
                          << ", bf=" << BalanceFactor(u) << ", ";
                if (item.side == 'O') {
                    std::cout << "root]";
                } else {
                    std::cout << item.side << " of " << item.parent_id << "]";
                }
                if (i + 1 < sz) std::cout << " ";

                if (u->left != nullptr) q.push(QueueItem{u->left, u->id, 'L'});
                if (u->right != nullptr) q.push(QueueItem{u->right, u->id, 'R'});
            }
            std::cout << "\n";
            ++level;
        }
    }

    /**
     * @brief Validates BST ordering, exact node heights, and |bf| <= delta.
     */
    bool Verify() const {
    bool VerifyAVL() const {
        return VerifyRec(root_,
                         std::numeric_limits<long long>::min(),
                         std::numeric_limits<long long>::max());
    }

    // Generic OOP method aliases
    inline void Insert(int id, int score) { InsertAVL(id, score); }
    inline double SearchAVG(int id) const { return SearchAVGAVL(id); }
    inline int Height() const noexcept { return HeightAVL(); }
    inline void Print() const { PrintAVL(); }
    inline bool Verify() const { return VerifyAVL(); }
};

// ============================================================================
// 3. Treap (BST by ID + Max-Heap by Priority)
// 3. Treap (BST by ID + Min-Heap by Priority in (0, 1))
// ============================================================================

class Treap {
private:
    struct Node {
        int id;
        long long sum_score;
        int count;
        double priority;
        Node* left;
        Node* right;
        Node* parent;

        Node(int node_id, int score, double prio)
        Node(int node_id, int score, double prio, Node* parent_node)
            : id(node_id),
              sum_score(score),
              count(1),
              priority(prio),
              left(nullptr),
              right(nullptr) {
              right(nullptr),
              parent(parent_node) {
            MemoryTracker::OnAllocate();
        }

        ~Node() {
            MemoryTracker::OnDeallocate();
        }
    };

    Node* root_;
    std::mt19937_64 rng_;
    std::uniform_real_distribution<double> prio_dist_;
    std::uniform_int_distribution<std::uint64_t> open_unit_dist_;

    static Node* RotateRight(Node* y) noexcept {
    /**
     * @brief Generates a uniform random double strictly inside the open interval (0, 1).
     */
    inline double NextOpenUnitPriority() {
        constexpr double kScale = 1.0 / static_cast<double>(1ULL << 53);
        return static_cast<double>(open_unit_dist_(rng_)) * kScale;
    }

    void RotateRightTreap(Node* y) noexcept {
        Node* x = y->left;
        y->left = x->right;
        Node* t2 = x->right;
        Node* g = y->parent;

        x->parent = g;
        if (g == nullptr) {
            root_ = x;
        } else if (y == g->left) {
            g->left = x;
        } else {
            g->right = x;
        }

        y->left = t2;
        if (t2 != nullptr) {
            t2->parent = y;
        }

        x->right = y;
        return x;
        y->parent = x;
    }

    static Node* RotateLeft(Node* x) noexcept {
    void RotateLeftTreap(Node* x) noexcept {
        Node* y = x->right;
        x->right = y->left;
        Node* t2 = y->left;
        Node* g = x->parent;

        y->parent = g;
        if (g == nullptr) {
            root_ = y;
        } else if (x == g->left) {
            g->left = y;
        } else {
            g->right = y;
        }

        x->right = t2;
        if (t2 != nullptr) {
            t2->parent = x;
        }

        y->left = x;
        return y;
        x->parent = y;
    }

    /**
     * @brief Recursive insertion preserving BST order on id and Max-Heap order on priority.
     *        Priority is only generated/assigned when allocating a brand-new node;
     *        duplicate IDs simply update sum_score and count in place.
     */
    Node* InsertRec(Node* node, int id, int score, const double* fixed_priority) {
        if (node == nullptr) {
            double prio = (fixed_priority != nullptr) ? (*fixed_priority) : prio_dist_(rng_);
            return new Node(id, score, prio);
    void InsertImplTreap(int id, int score, const double* fixed_priority) {
        if (root_ == nullptr) {
            double prio = (fixed_priority != nullptr) ? (*fixed_priority)
                                                      : NextOpenUnitPriority();
            root_ = new Node(id, score, prio, nullptr);
            return;
        }

        if (id == node->id) {
            node->sum_score += score;
            node->count += 1;
            return node;
        } else if (id < node->id) {
            node->left = InsertRec(node->left, id, score, fixed_priority);
            if (node->left->priority > node->priority) {
                node = RotateRight(node);
        // Phase 1: Standard BST downward search to find existing ID or leaf slot
        Node* curr = root_;
        Node* parent = nullptr;
        while (curr != nullptr) {
            parent = curr;
            if (id == curr->id) {
                curr->sum_score += score;
                curr->count += 1;
                return;  // Duplicate ID updated in place; priority & structure unchanged
            } else if (id < curr->id) {
                curr = curr->left;
            } else {
                curr = curr->right;
            }
        }

        double prio = (fixed_priority != nullptr) ? (*fixed_priority)
                                                  : NextOpenUnitPriority();
        Node* new_node = new Node(id, score, prio, parent);
        if (id < parent->id) {
            parent->left = new_node;
        } else {
            node->right = InsertRec(node->right, id, score, fixed_priority);
            if (node->right->priority > node->priority) {
                node = RotateLeft(node);
            parent->right = new_node;
        }

        // === [REPORT HIGHLIGHT: Treap upward-rotation loop (Min-Heap)] ===
        while (new_node->parent != nullptr &&
               new_node->priority < new_node->parent->priority) {
            Node* p = new_node->parent;
            if (new_node == p->left) {
                RotateRightTreap(p);
            } else {
                RotateLeftTreap(p);
            }
        }
        return node;
        // === [END REPORT HIGHLIGHT] ===
    }

    static int ComputeHeightRec(const Node* node) noexcept {
    static int ComputeHeightBFS(const Node* start) {
        if (start == nullptr) return 0;
        std::queue<const Node*> q;
        q.push(start);
        int levels = 0;
        while (!q.empty()) {
            std::size_t sz = q.size();
            ++levels;
            for (std::size_t i = 0; i < sz; ++i) {
                const Node* u = q.front();
                q.pop();
                if (u->left != nullptr) q.push(u->left);
                if (u->right != nullptr) q.push(u->right);
            }
        }
        return levels;
    }

    static int ComputeHeightRec(const Node* node, int depth = 0) {
        if (node == nullptr) return 0;
        return 1 + std::max(ComputeHeightRec(node->left), ComputeHeightRec(node->right));
        if (depth > 4096) return ComputeHeightBFS(node);
        return 1 + std::max(ComputeHeightRec(node->left, depth + 1),
                            ComputeHeightRec(node->right, depth + 1));
    }

    /**
     * @brief Strictly frees all nodes using post-order traversal (Left, Right, Root).
     */
    static void DestroyPostOrder(Node* node) {
    static void DestroyPostOrderIterative(Node* subtree_root) {
        if (subtree_root == nullptr) return;
        std::vector<Node*> stack;
        stack.push_back(subtree_root);
        Node* last_deleted = nullptr;

        while (!stack.empty()) {
            Node* curr = stack.back();
            bool left_done = (curr->left == nullptr);
            bool right_done = (curr->right == nullptr);
            if (curr->right != nullptr && curr->right == last_deleted) {
                curr->right = nullptr;
                right_done = true;
                left_done = true;
            } else if (curr->left != nullptr && curr->left == last_deleted) {
                curr->left = nullptr;
                left_done = true;
            }

            if (!left_done) {
                stack.push_back(curr->left);
            } else if (!right_done) {
                stack.push_back(curr->right);
            } else {
                stack.pop_back();
                last_deleted = curr;
                delete curr;
            }
        }
    }

    static void DestroyPostOrderTreap(Node* node, int depth = 0) {
        if (node == nullptr) return;
        DestroyPostOrder(node->left);
        DestroyPostOrder(node->right);
        if (depth > 4096) {
            DestroyPostOrderIterative(node);
            return;
        }
        DestroyPostOrderTreap(node->left, depth + 1);
        DestroyPostOrderTreap(node->right, depth + 1);
        delete node;
    }

    static bool VerifyRec(const Node* node, long long min_id, long long max_id) {
    static bool VerifyRec(const Node* node,
                          const Node* expected_parent,
                          long long min_id,
                          long long max_id) {
        if (node == nullptr) return true;
        if (node->parent != expected_parent) return false;
        if (node->id <= min_id || node->id >= max_id || node->count <= 0) {
            return false;
        }
        if (node->left != nullptr && node->left->priority > node->priority) {
        // Min-Heap property: parent priority <= child priority
        if (node->left != nullptr && node->left->priority < node->priority) {
            return false;
        }
        if (node->right != nullptr && node->right->priority > node->priority) {
        if (node->right != nullptr && node->right->priority < node->priority) {
            return false;
        }
        return VerifyRec(node->left, min_id, node->id) &&
               VerifyRec(node->right, node->id, max_id);
        return VerifyRec(node->left, node, min_id, node->id) &&
               VerifyRec(node->right, node, node->id, max_id);
    }

public:
    explicit Treap(std::uint64_t seed = 5489ULL)
        : root_(nullptr), rng_(seed), prio_dist_(0.0, 1.0) {}
        : root_(nullptr),
          rng_(seed),
          open_unit_dist_(1ULL, (1ULL << 53) - 1ULL) {}

    // CreateTreap OOP equivalent
    Treap(int id, int score, std::uint64_t seed = 5489ULL)
        : root_(nullptr),
          rng_(seed),
          open_unit_dist_(1ULL, (1ULL << 53) - 1ULL) {
        InsertTreap(id, score);
    }

    Treap(const Treap&) = delete;
    Treap& operator=(const Treap&) = delete;

    /**
     * @brief Destructor strictly freeing all nodes in post-order.
     */
    ~Treap() {
        DestroyPostOrder(root_);
        DestroyPostOrderTreap(root_);
        root_ = nullptr;
    }

    /**
     * @brief Standard insertion with uniformly random priority in [0, 1).
     */
    void Insert(int id, int score) {
        root_ = InsertRec(root_, id, score, nullptr);
    void InsertTreap(int id, int score) {
        InsertImplTreap(id, score, nullptr);
    }

    /**
     * @brief Testing-friendly insertion accepting a fixed priority from outside.
     */
    void Insert(int id, int score, double fixed_priority) {
        root_ = InsertRec(root_, id, score, &fixed_priority);
    void InsertTreap(int id, int score, double fixed_priority) {
        InsertImplTreap(id, score, &fixed_priority);
    }

    double SearchAVG(int id) const {
    double SearchAVGTreap(int id) const {
        const Node* curr = root_;
        while (curr != nullptr) {
            if (id == curr->id) {
                return static_cast<double>(curr->sum_score) / curr->count;
            } else if (id < curr->id) {
                curr = curr->left;
            } else {
                curr = curr->right;
            }
        }
        return -1.0;
    }

    /**
     * @brief Returns the number of levels in the Treap (0 if empty).
     */
    int Height() const noexcept {
        return ComputeHeightRec(root_);
    int HeightTreap() const {
        return ComputeHeightRec(root_, 0);
    }

    void Print() const {
        std::cout << "[Treap (Max-Heap Priority)] Total Levels (Height) = "
                  << Height() << "\n";
    void PrintTreap() const {
        std::cout << "[Treap (Min-Heap)] Height = " << HeightTreap() << "\n";
        if (root_ == nullptr) {
            std::cout << "  (empty)\n";
            return;
        }

        struct QueueItem {
            const Node* node;
            int parent_id;
            char side;
        };

        std::queue<QueueItem> q;
        q.push(QueueItem{root_, -1, 'O'});
        int level = 1;

        while (!q.empty()) {
            std::size_t sz = q.size();
            std::cout << "  Level " << level << ": ";
            for (std::size_t i = 0; i < sz; ++i) {
                QueueItem item = q.front();
                q.pop();
                const Node* u = item.node;
                double avg = static_cast<double>(u->sum_score) / u->count;

                std::cout << "[id=" << u->id
                          << ", avg=" << std::fixed << std::setprecision(2) << avg
                          << " (cnt=" << u->count << ")"
                          << ", prio=" << std::fixed << std::setprecision(4) << u->priority
                          << ", ";
                if (item.side == 'O') {
                    std::cout << "root]";
                } else {
                    std::cout << item.side << " of " << item.parent_id << "]";
                }
                if (i + 1 < sz) std::cout << " ";

                if (u->left != nullptr) q.push(QueueItem{u->left, u->id, 'L'});
                if (u->right != nullptr) q.push(QueueItem{u->right, u->id, 'R'});
            }
            std::cout << "\n";
            ++level;
        }
    }

    /**
     * @brief Validates BST ordering on id and Max-Heap ordering on priority.
     */
    bool Verify() const {
    bool VerifyTreap() const {
        return VerifyRec(root_,
                         nullptr,
                         std::numeric_limits<long long>::min(),
                         std::numeric_limits<long long>::max());
    }

    // Generic OOP method aliases
    inline void Insert(int id, int score) { InsertTreap(id, score); }
    inline void Insert(int id, int score, double fixed_priority) {
        InsertTreap(id, score, fixed_priority);
    }
    inline double SearchAVG(int id) const { return SearchAVGTreap(id); }
    inline int Height() const { return HeightTreap(); }
    inline void Print() const { PrintTreap(); }
    inline bool Verify() const { return VerifyTreap(); }
};

// ============================================================================
// 4. Parameterized Skip List (Promotion Probability = p, NO Height Limit)
// 4. Parameterized Skip List (Probability = p, Unbounded Height)
// ============================================================================

class SkipList {
private:
    struct Node {
        int id;
        long long sum_score;
        int count;
        std::vector<Node*> forward;

        Node(int node_id, int score, int num_levels)
            : id(node_id),
              sum_score(score),
              count(1),
              forward(static_cast<std::size_t>(num_levels), nullptr) {
            MemoryTracker::OnAllocate();
        }

        ~Node() {
            MemoryTracker::OnDeallocate();
        }
    };

    Node* head_;  // Sentinel header node; head_->forward.size() == current active levels
    Node* head_;
    double prob_;
    std::mt19937_64 rng_;
    std::uniform_real_distribution<double> coin_dist_;
    std::vector<Node*> update_scratch_;  // Reusable predecessor buffer (avoids per-Insert allocs)
    std::vector<Node*> update_scratch_;

    /**
     * @brief Core insertion logic supporting random coin flips, explicit boolean
     *        coin-toss sequences, or a fixed count of successful promotions.
     */
    void InsertImpl(int id, int score, const std::vector<bool>* fixed_tosses, int fixed_heads) {
    void InsertImplSkipList(int id,
                            int score,
                            const std::vector<bool>* fixed_tosses,
                            const std::string* coin_str) {
        int current_h = static_cast<int>(head_->forward.size());
        if (static_cast<int>(update_scratch_.size()) < current_h) {
            update_scratch_.resize(static_cast<std::size_t>(current_h), head_);
        }

        Node* curr = head_;
        for (int lvl = current_h - 1; lvl >= 0; --lvl) {
            while (curr->forward[lvl] != nullptr && curr->forward[lvl]->id < id) {
                curr = curr->forward[lvl];
            }
            update_scratch_[lvl] = curr;
        }

        // One-node-per-ID duplicate check at base level 0
        // Duplicate ID check at Level 1 (index 0)
        Node* candidate = (current_h > 0) ? update_scratch_[0]->forward[0] : nullptr;
        if (candidate != nullptr && candidate->id == id) {
            candidate->sum_score += score;
            candidate->count += 1;
            return;
        }

        // Determine tower height (number of levels >= 1) with NO artificial ceiling
        // Determine number of levels (bottom-most base list = 1 level)
        int node_levels = 1;
        if (fixed_tosses != nullptr) {
        if (coin_str != nullptr) {
            for (std::size_t i = 0; i < coin_str->size(); ++i) {
                char c = (*coin_str)[i];
                if (c == 'H' || c == 'h' || c == '1') {
                    ++node_levels;
                } else if (c == 'T' || c == 't' || c == '0') {
                    break;
                } else {
                    throw std::invalid_argument(
                        std::string("Invalid coin-toss character: ") + c);
                }
            }
        } else if (fixed_tosses != nullptr) {
            for (std::size_t i = 0; i < fixed_tosses->size(); ++i) {
                if ((*fixed_tosses)[i]) {
                    ++node_levels;
                } else {
                    break;  // First tails stops promotion
                    break;
                }
            }
        } else if (fixed_heads >= 0) {
            node_levels = 1 + fixed_heads;
        } else {
            // === [REPORT HIGHLIGHT: SkipList coin-flip loop] ===
            while (prob_ > 0.0 && coin_dist_(rng_) < prob_) {
                ++node_levels;
                ++node_levels;  // Heads (prob p): grow one more level; stop on first Tails
            }
            // === [END REPORT HIGHLIGHT] ===
        }

        // Dynamically grow sentinel head & scratch buffer if new tower exceeds current height
        // Dynamically grow header without any fixed ceiling (e.g., no 32-level cap)
        if (node_levels > current_h) {
            head_->forward.resize(static_cast<std::size_t>(node_levels), nullptr);
            if (static_cast<int>(update_scratch_.size()) < node_levels) {
                update_scratch_.resize(static_cast<std::size_t>(node_levels), head_);
            }
            for (int lvl = current_h; lvl < node_levels; ++lvl) {
                update_scratch_[lvl] = head_;
            }
        }

        Node* new_node = new Node(id, score, node_levels);
        for (int lvl = 0; lvl < node_levels; ++lvl) {
            new_node->forward[lvl] = update_scratch_[lvl]->forward[lvl];
            update_scratch_[lvl]->forward[lvl] = new_node;
        }
    }

    /**
     * @brief Strictly frees all nodes in post-order (tail -> ... -> first -> head).
     *
     * Viewing level 0 (which links every allocated node) as a unary chain from
     * head_ to tail, recursive post-order on N = 2^20 nodes would overflow the
     * call stack. Reversing the level-0 links in-place in O(N) time and O(1)
     * extra memory allows deleting strictly from the last node back to head_
     * (every successor freed before its predecessor) with zero stack growth.
     * @brief Strictly frees all nodes in post-order (tail -> ... -> first -> head)
     *        using O(1)-space in-place level-0 link reversal to avoid stack overflow.
     */
    void DestroyPostOrder() {
    void DestroyPostOrderSkipList() {
        if (head_ == nullptr) return;
        if (head_->forward.empty()) {
            delete head_;
            head_ = nullptr;
            return;
        }

        // Pass 1: Reverse level-0 forward links in-place
        Node* prev = nullptr;
        Node* curr = head_;
        while (curr != nullptr) {
            Node* next = curr->forward[0];
            curr->forward[0] = prev;
            prev = curr;
            curr = next;
        }

        // Pass 2: Delete from tail back to head_ (strict post-order)
        curr = prev;
        while (curr != nullptr) {
            Node* predecessor = curr->forward[0];
            delete curr;
            curr = predecessor;
        }
        head_ = nullptr;
    }

public:
    explicit SkipList(double p = 0.5, std::uint64_t seed = 5489ULL)
        : head_(nullptr), prob_(p), rng_(seed), coin_dist_(0.0, 1.0), update_scratch_() {
        if (p < 0.0 || p >= 1.0) {
            throw std::invalid_argument("SkipList probability p must satisfy 0.0 <= p < 1.0.");
        }
        head_ = new Node(0, 0, 0);  // Empty list starts with 0 active levels
        head_ = new Node(0, 0, 0);
    }

    // CreateSkipList OOP equivalent
    SkipList(int id, int score, double p = 0.5, std::uint64_t seed = 5489ULL)
        : head_(nullptr), prob_(p), rng_(seed), coin_dist_(0.0, 1.0), update_scratch_() {
        if (p < 0.0 || p >= 1.0) {
            throw std::invalid_argument("SkipList probability p must satisfy 0.0 <= p < 1.0.");
        }
        head_ = new Node(0, 0, 0);
        InsertSkipList(id, score);
    }

    SkipList(const SkipList&) = delete;
    SkipList& operator=(const SkipList&) = delete;

    /**
     * @brief Destructor strictly freeing all nodes in post-order.
     */
    ~SkipList() {
        DestroyPostOrder();
        DestroyPostOrderSkipList();
    }

    double GetProbability() const noexcept { return prob_; }

    /**
     * @brief Standard insertion flipping a p-biased coin until first failure.
     */
    void Insert(int id, int score) {
        InsertImpl(id, score, nullptr, -1);
    void InsertSkipList(int id, int score) {
        InsertImplSkipList(id, score, nullptr, nullptr);
    }

    /**
     * @brief Testing-friendly insertion accepting fixed coin-toss results
     *        (true = heads / promote, false = tails / stop).
     */
    void Insert(int id, int score, const std::vector<bool>& coin_tosses) {
        InsertImpl(id, score, &coin_tosses, -1);
    void InsertSkipList(int id, int score, const std::string& coin_results) {
        InsertImplSkipList(id, score, nullptr, &coin_results);
    }

    void InsertSkipList(int id, int score, const char* coin_results) {
        std::string s(coin_results != nullptr ? coin_results : "");
        InsertImplSkipList(id, score, nullptr, &s);
    }

    void InsertSkipList(int id, int score, const std::vector<bool>& coin_tosses) {
        InsertImplSkipList(id, score, &coin_tosses, nullptr);
    }

    /**
     * @brief Testing-friendly overload accepting a fixed count of heads (>= 0).
     * @brief Zero-counter fast SearchAVG used in Experiment 2 timing.
     *        Since each ID is stored in a single node, stops immediately upon
     *        reaching the matching node at its highest level.
     */
    void Insert(int id, int score, int fixed_heads) {
        if (fixed_heads < 0) {
            throw std::invalid_argument("fixed_heads must be >= 0.");
    double SearchAVGSkipList(int id) const {
        if (head_ == nullptr || head_->forward.empty()) {
            return -1.0;
        }
        InsertImpl(id, score, nullptr, fixed_heads);

        const Node* curr = head_;
        int top_lvl = static_cast<int>(head_->forward.size()) - 1;
        for (int lvl = top_lvl; lvl >= 0; --lvl) {
            while (curr->forward[lvl] != nullptr && curr->forward[lvl]->id < id) {
                curr = curr->forward[lvl];
            }
            if (curr->forward[lvl] != nullptr && curr->forward[lvl]->id == id) {
                const Node* hit = curr->forward[lvl];
                return static_cast<double>(hit->sum_score) / hit->count;
            }
        }
        return -1.0;
    }

    double SearchAVG(int id) const {
    /**
     * @brief Step-counting SearchAVG used ONLY in Experiment 3 Question 4(a)
     *        (kept separate so Experiment 2 timing has zero counter overhead).
     *        Moving right to the next node = +1 step; moving down one level = +1 step.
     */
    double SearchAVGWithStepsSkipList(int id, std::uint64_t& steps_out) const {
        steps_out = 0;
        if (head_ == nullptr || head_->forward.empty()) {
            return -1.0;
        }

        const Node* curr = head_;
        int top_lvl = static_cast<int>(head_->forward.size()) - 1;
        for (int lvl = top_lvl; lvl >= 0; --lvl) {
            while (curr->forward[lvl] != nullptr && curr->forward[lvl]->id < id) {
                curr = curr->forward[lvl];
                ++steps_out;  // Moved right to next node
            }
            if (curr->forward[lvl] != nullptr && curr->forward[lvl]->id == id) {
                const Node* hit = curr->forward[lvl];
                ++steps_out;  // Moved right onto matching node
                return static_cast<double>(hit->sum_score) / hit->count;
            }
            if (lvl > 0) {
                ++steps_out;  // Dropped down one level
            }
        }

        const Node* target = curr->forward[0];
        if (target != nullptr && target->id == id) {
            return static_cast<double>(target->sum_score) / target->count;
        }
        return -1.0;
    }

    /**
     * @brief Returns the number of active levels in the SkipList (0 if empty).
     * @brief Returns total active levels (empty = 0; bottom base list = Level 1).
     */
    int Height() const noexcept {
    int HeightSkipList() const noexcept {
        return (head_ != nullptr) ? static_cast<int>(head_->forward.size()) : 0;
    }

    /**
     * @brief Prints the SkipList horizontally from the highest level down to Level 0.
     * @brief Prints SkipList layer-by-layer from highest level (Level H) down to
     *        the base list (Level 1), matching the PDF's 1-based level definition.
     */
    void Print() const {
        int h = Height();
    void PrintSkipList() const {
        int h = HeightSkipList();
        std::cout << "[SkipList (p=" << std::fixed << std::setprecision(2) << prob_
                  << ")] Total Levels (Height) = " << h << "\n";
                  << ")] Height = " << h << "\n";
        if (h == 0) {
            std::cout << "  (empty)\n";
            return;
        }

        for (int lvl = h - 1; lvl >= 0; --lvl) {
            std::cout << "  Level " << lvl << ": HEAD -> ";
            std::cout << "  Level " << (lvl + 1) << ": HEAD -> ";
            const Node* curr = head_->forward[lvl];
            while (curr != nullptr) {
                double avg = static_cast<double>(curr->sum_score) / curr->count;
                std::cout << "[id=" << curr->id
                          << ", avg=" << std::fixed << std::setprecision(2) << avg
                          << " (cnt=" << curr->count << ")] -> ";
                curr = curr->forward[lvl];
            }
            std::cout << "NIL\n";
        }
    }

    /**
     * @brief Validates strict ascending order across every level and tower consistency.
     */
    bool Verify() const {
    bool VerifySkipList() const {
        if (head_ == nullptr) return false;
        int h = Height();
        int h = HeightSkipList();
        for (int lvl = 0; lvl < h; ++lvl) {
            const Node* curr = head_->forward[lvl];
            long long prev_id = std::numeric_limits<long long>::min();
            if (lvl == h - 1 && curr == nullptr) {
                return false;  // Top active level must contain at least one node
            }
            if (lvl == h - 1 && curr == nullptr) return false;
            while (curr != nullptr) {
                if (curr->id <= prev_id || curr->count <= 0) return false;
                if (static_cast<int>(curr->forward.size()) <= lvl) return false;
                prev_id = curr->id;
                curr = curr->forward[lvl];
            }
        }
        return true;
    }

    // Generic OOP method aliases
    inline void Insert(int id, int score) { InsertSkipList(id, score); }
    inline void Insert(int id, int score, const std::string& coin_results) {
        InsertSkipList(id, score, coin_results);
    }
    inline void Insert(int id, int score, const char* coin_results) {
        InsertSkipList(id, score, coin_results);
    }
    inline void Insert(int id, int score, const std::vector<bool>& coin_tosses) {
        InsertSkipList(id, score, coin_tosses);
    }
    inline double SearchAVG(int id) const { return SearchAVGSkipList(id); }
    inline int Height() const noexcept { return HeightSkipList(); }
    inline void Print() const { PrintSkipList(); }
    inline bool Verify() const { return VerifySkipList(); }
};

}  // namespace algo_hw2

// Expose required classes directly in global namespace for assignment compatibility
using algo_hw2::AVL;
using algo_hw2::BST;
using algo_hw2::SkipList;
using algo_hw2::Treap;

#endif  // DATA_STRUCTURES_H

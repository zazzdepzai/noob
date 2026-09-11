#pragma once
#include <string>
#include <vector>

struct Account {
    std::string name;
    std::string uuid;
    bool        active = false;
    int         colorIdx = 0;   // màu avatar
};

class AccountManager {
public:
    static AccountManager& I();

    void load();
    void save();

    const std::vector<Account>& list() const { return m_list; }
    std::vector<Account>& list() { return m_list; }

    // Trả về tên account đang active (hoặc "Player")
    std::string activeName() const;

    // Trả về index account active, -1 nếu không có
    int activeIndex() const;

    void add(const std::string& name);
    void remove(size_t idx);
    void setActive(size_t idx);
    void rename(size_t idx, const std::string& newName);

    // Màu avatar từ colorIdx
    static void avatarColor(int idx, int& r, int& g, int& b);

private:
    AccountManager() = default;
    std::vector<Account> m_list;
};

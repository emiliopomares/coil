// Fixture for gate.tidy_rejects_bad_naming: the private member violates the m_camelCase rule.
class Widget {
public:
    [[nodiscard]] int value() const { return BadName; }

private:
    int BadName = 0;
};

int main() {
    return Widget{}.value();
}

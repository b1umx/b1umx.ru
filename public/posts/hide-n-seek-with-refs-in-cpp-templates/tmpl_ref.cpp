#include <type_traits>
#include <iostream>
#include <string>
#include <sstream>
#include <memory>


class Entity {
public:
    virtual void save() const = 0;
    virtual std::string toString() const = 0;
};


template<typename T>
class Restorable: public Entity {
public:
    Restorable(const T& value): 
        _actual(value),
        _original(value) {
    }

    bool isChanged() const {
        return _actual != _original;
    }

    void restore() {
        _actual = _original;
    }

    const T& value() const {
        return _actual;
    }

    T& value() {
        return _actual;
    }

    void save() const override {
        return _actual.save();
    }

    std::string toString() const override {
        return _actual.toString();
    }

private:
    T _actual;
    T _original;
};


template<typename T>
Restorable<std::decay_t<T>> makeRestorable(T&& sample) {
	sample.save();
	return Restorable<std::decay_t<T>>(std::forward<T>(sample));
}


template<typename T>
std::unique_ptr<Entity> makeEntity(T&& sample) {
	return std::make_unique<Restorable<std::decay_t<T>>>(makeRestorable(std::forward<T>(sample)));
}



struct S {
    int id = 0;
    std::string name;

    void save() const {
        std::cout << "Сохраняю S #" << id << "...\n";
        // ...
    }

    std::string toString() const {
        std::stringstream ss;
        ss << "S[ #" << std::to_string(id) << ' ' << name << " ]";
        return ss.str();
    } 
};


std::unique_ptr<Entity> loadEntity() {
    S s { 3, "gamma" };
    return makeEntity(s);
}



int main() {
    S s;
    
    s = { 1, "alpha" };
	auto r1 = makeRestorable(s);
    // std::cout 
    //     << std::boolalpha
    //     << "Тип r1 - Restorable<S>? - "
    //     << std::is_same_v<decltype(r1), Restorable<S>> << '\n'
    //     << "Тип r1 - Restorable<S&>? - "
    //     << std::is_same_v<decltype(r1), Restorable<S&>> << '\n';
	std::cout << "r1 = " << r1.toString() << '\n';


    s = { 2, "beta" };
	auto r2 = makeRestorable(s);
	std::cout << "r2 = " << r2.toString() << "\n\n";

    std::cout << "r1 = " << r1.toString() << '\n';
    std::cout << "r2 = " << r2.toString() << "\n\n";

    auto r3 = loadEntity();
    std::cout << "r3 = " << r3->toString() << "\n\n";

    const S cs { 4, "delta" };
    auto r4 = makeRestorable(cs);
    // std::cout
	// 	<< std::boolalpha
	// 	<< "Тип r4 - Restorable<const S>? - "
	// 	<< std::is_same_v<decltype(r4), Restorable<const S>> << '\n';
    std::cout << "r4 = " << r4.toString() << '\n';


    r4.value().name = "delta (upd.)";
    std::cout << "r4 = " << r4.toString() << '\n';

    // std::cout 
    //     << std::boolalpha
    //     << std::is_same_v<void (int ()), void (int (*)())> << '\n'
    //     << std::is_same_v<void (int [3]), void (int *)> << '\n';
}

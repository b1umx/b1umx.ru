---
title: "Прятки со ссылками в шаблонах C++"
date: 2022-03-16
cover: cover.png
coverAlt: std::decay
useRelativeCover: true
category: Программирование
tags:
  - C++
  - templates
  - bugs
description: Когда я начал изучать шаблоны в C++, то часто натыкался на использование *метафункций* `std::decay`, `std::remove_reference` и `std::remove_cv`. Однако искусство их применения от меня ускользало. Я не мог прочувствовать те моменты, когда пора расчехлять эти инструменты... До одного вечера, проведенного за отладчиком.
---

Когда я начал изучать шаблоны в C++, то часто натыкался на использование *метафункций* `std::decay`, `std::remove_reference` и `std::remove_cv`. Однако искусство их применения от меня ускользало. Я не мог прочувствовать те моменты, когда пора расчехлять эти инструменты... До одного вечера, проведенного за отладчиком.

Придумал я для рабочего проекта класс. Использовал технику *стирания типа (type erasure)*. Как в `std::function`, только вместо `operator()` мне нужны были другие функции в интерфейсе. Об этом решении как-нибудь в следующий раз расскажу. Написал рабочую реализацию, начал тестировать, а один тест падает с segmentation fault. Ну, дело обычное, подумал я и включил отладчик. Не тут-то было! Где происходит падение, отследить не получается: стек вызовов ни о чем не говорит. И так вертел я, и сяк, отлаживал пошагово, ставил выводы в лог в разных точках - ничего не помогает. Полдня потратил на поиски причины ошибки. В какой-то момент обложил принтами в лог все, что только возможно и, наконец, нашел. Ошибка оказалась до смешного классической - висячая сслыка. 

Проблема возникла из-за шаблонной функции с аргументом в виде универсальной ссылки. Внутри функции конструировался объект в динамической памяти, и в вызывающий код передавался как указатель на родительский класс. Схематично проиллюстрировать это можно следующим образом

{{< figure src="hang_ref_schema.png" alt="Schema" position="center" style="border-radius: 8px;">}}

Покончим с прелюдией. В тот вечер я смог продвинуться в понимании метафункций стандартной библиотеки и теперь хочу поделиться своим опытом. От слов к примерам!

Одно из типичных применений шаблона класса - контейнер, который добавляет хранимому объекту функциональность, не связанную с типом этого объекта. Представим, что мы хотим отслеживать факт изменения объектов различных полезных типов и при необходимости возвращать их к исходному состоянию. Создадим шаблонный класс-обертку `Restorable<T>`. Он будет наследовать абстрактному классу `Entity`, в котором мы задекларируем интерфейс.

{{<code language="cpp" title="Restorable">}}
class Entity {
public:
	virtual void save() const = 0;
	virtual std::string toString() const = 0;
};


template<typename T>
class Restorable: public Entity {
public:
	Restorable(const T &value):
		_actual(value),
		_original(value) {
	}

	bool isChanged() const {
		return _actual != _original;
	}

	void restore() {
		_actual = _original;
	}  

	const T & value() const {
		return _actual;
	}

	T & value() {
		return _actual;
	}
	
	void save() const override {
		return _actual.save();
	}
	
	std::string toString() const override {
		return _actual.toString();
	}
}

private:
	T _actual;
	T _original;
};
{{</code>}}

*От оборачиваемого типа нам потребуются функции-члены `save()` и `toString()`, а также оператор неравенства `!=`, копирующий конструктор и оператор копирующего присваивания.*

Допустим, что перед созданием `Restorable` значений мы хотим дополнительно обрабатывать все объекты. Тогда нам понадобиться универсальная функция `makeRestorable<T>`. В подобной функции можно, например, сгенерировать ID объекта, вывести полезную информацию в лог или сохранить объект в базе данных. Заодно сделаем еще более абстрактную функцию `makeEntity<T>`, которая будет возвращать указатель на объект `Entity`, созданный в динамической памяти.

{{<code language="cpp" title="make-функции">}}
template<typename T>
Restorable<T> makeRestorable(T &&sample) {
	sample.save();
	return Restorable<T>(std::forward<T>(sample));
}

template<typename T>
std::unique_ptr<Entity> makeEntity(T &&sample) {
	return std::make_unique<Restorable<T>>(makeRestorable(std::forward<T>(sample)));
}
{{</code>}}

Пока реализация выглядит стройной. Мы довольны собой и пробуем поиграть с написанным классом.

{{<code language="cpp">}}
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

int main() {
	S s;

	s = { 1, "alpha" };
	auto r1 = makeRestorable(s);
	std::cout << "r1 = " << r1.toString() << '\n';

	s = { 2, "beta" };
	auto r2 = makeRestorable(s);
	std::cout << "r2 = " << r2.toString() << "\n\n";

	std::cout << "r1 = " << r1.toString() << '\n';
	std::cout << "r2 = " << r2.toString() << "\n\n";
}
{{</code>}}

Вывод программы вас удивит, если вы не закаленный шаблонами программист. По какой-то загадочной причине после создания `r2` изменилось значение в `r1`.

```txt
Сохраняю S #1...  
r1 = S[ #1 alpha ]  
Сохраняю S #2...  
r2 = S[ #2 beta ]  
  
r1 = S[ #2 beta ]  
r2 = S[ #2 beta ]
```

Давайте еще усугубим ситуацию. Представим, что нам надо реализовать загрузку объектов по сети. Смоделируем это поведение с помощью своеобразной функции `loadEntity`.

{{<code language="cpp">}}
std::unique_ptr<Entity> loadEntity() {
	S s { 3, "gamma" };
	return makeEntity(s);
}

int main() {
	// ...
	
	auto r3 = loadEntity();
	std::cout << "r3 = " << r3->toString() << "\n\n";
}
{{</code>}}

Вот и обещанный сегфолт!

```txt
...
Сохраняю S #3...
zsh: segmentation fault (core dumped)  ./tmpl_ref
```

В чем же причина такого поведения? Взглянем еще раз на пример с `r1` и `r2`. После создания `r2`, а точнее после присвоения нового значения переменной `s` изменилось и значение внутри `r1`. Интуиция может подсказать, что в деле замешаны ссылки. Давайте проверим нашу гипотезу. Проанализируем инстанцированный тип объекта `r1`.

{{<code language="cpp">}}
int main() {
	S s;
	
	s = { 1, "alpha" };
	auto r1 = makeRestorable(s);
	std::cout
		<< std::boolalpha
		<< "Тип r1 - Restorable<S>? - "
		<< std::is_same_v<decltype(r1), Restorable<S>> << '\n'
		<< "Тип r1 - Restorable<S&>? - "
		<< std::is_same_v<decltype(r1), Restorable<S&>> << '\n';
	
	// выведет:
	// Сохраняю S #1...
	// Тип r1 - Restorable<S>? - false
    // Тип r1 - Restorable<S&>? - true
}
{{</code>}}

Так и есть. Вместо обертки для объекта типа `S` мы получили обертку для ссылки на объект `S&`. Это явно не то, что мы подразумевали. Нам нужен способ сказать компилятору, что нам нужны "чистые" значения, даже если в функцию передаются значения по ссылке. Для этих целей в стандартной библиотеке предусмотрена метафункция `std::remove_reference`. Применим ее.

{{<code language="cpp" title="std::remove_reference">}}
template<typename T>
Restorable<std::remove_reference_t<T>> makeRestorable(T&& sample) {
	sample.save();
	return Restorable<std::remove_reference_t<T>>(std::forward<T>(sample));
}

template<typename T>
std::unique_ptr<Entity> makeEntity(T&& sample) {
	return std::make_unique<Restorable<std::remove_reference_t<T>>>(makeRestorable(std::forward<T>(sample)));
}
{{</code>}}

После запуска обновленной программы, терминал нам покажет следующий вывод

```txt
Сохраняю S #1...  
r1 = S[ #1 alpha ]  
Сохраняю S #2...  
r2 = S[ #2 beta ]  
  
r1 = S[ #1 alpha ]  
r2 = S[ #2 beta ]  
  
Сохраняю S #3...  
r3 = S[ #3 gamma ]
```

Победа! Но полная ли? Что если мы недавно прочитали "Эффективное использование C++" Скотта Мэйерса и помним правило 3: везде использовать `const`? А тут еще и осадочек остался после работы с изменяемой переменной `s` из предыдущего примера.

{{<code language="cpp">}}
int main() {
	// ...
	
	const S cs { 4, "delta" };
	auto r4 = makeRestorable(cs);
	std::cout << "r4 = " << r4.toString() << '\n';
	
	r4.value().name = "delta (upd.)";
	std::cout << "r4 = " << r4.toString() << '\n';
}
{{</code>}}

Компилятор спешит вас разочаровать огромной простыней ошибок. Самая суть в этих строчках

```txt
decay.cpp: In function ‘int main()’:  
decay.cpp:176:49: error: no match for ‘operator=’ (operand types are ‘const string’ {aka ‘const std::__cxx11::basic_string<char>’} a  
nd ‘std::string’ {aka ‘std::__cxx11::basic_string<char>’})  
 176 |     r4.value().name = std::string("delta (upd.)");  
     |
```

Мы не можем присвоить `const` члену объекта новое значение. Оказывается, обобщенный тип `T` может вычисляться не только как ссылка на тип, но и как неизменный объект `[T = const S]`. 

{{<code language="cpp">}}
int main() {
	// ...
	
	const S cs { 4, "delta" };
	auto r4 = makeRestorable(cs);
	std::cout
		<< std::boolalpha
		<< "Тип r4 - Restorable<const S>? - "
		<< std::is_same_v<decltype(r4), Restorable<const S>> << '\n';
	
	// выводит:
	// Сохраняю S #4...  
	// Тип r4 - Restorable<const S>? - true
}
{{</code>}}

Чтобы окончательно исправить ситуацию, позовем верного соратника `std::remove_reference` – метафункцию `std::remove_cv`. Она удаляет у типа квалификаторы `const` и `volatile`.

{{<code language="cpp" title="std::remove_cv">}}
template<typename T>
Restorable<std::remove_cv_t<std::remove_reference_t<T>>> makeRestorable(T&& sample) {
	sample.save();
	return Restorable<std::remove_cv_t<std::remove_reference_t<T>>>(std::forward<T>(sample));
}

template<typename T>
std::unique_ptr<Entity> makeEntity(T&& sample) {
	return std::make_unique<Restorable<std::remove_cv_t<std::remove_reference_t<T>>>>(makeRestorable(std::forward<T>(sample)));
}
{{</code>}}

Теперь наш код компилируется и печатает в терминал то, что мы хотим.

```txt
Сохраняю S #4...  
r4 = S[ #4 delta ]  
r4 = S[ #4 delta (upd.) ]
```

В принципе, мы уже получили рабочий код. Но кое-что можно еще улучшить.

Универсальная ссылка `T &&` в качестве аргумента функции позволяет реализовать оптимальную передачу параметра в двух случаях одновременно.

{{<code language="cpp" title="std::remove_cv">}}
S s { 1, "alpha" };
auto r1 = makeRestorable(s);
// функция ведет себя, как если бы она имела вид makeRestorable(const S &);

auto r2 = makeRestorable(S { 2, "beta" });
// функция ведет себя, как если бы она имела вид makeRestorable(S);
{{</code>}}

*Обычно в таких случаях говорят об **lvalue** и **rvalue**, но об этом как-нибудь в следующий раз.* Реализация через универсальную ссылку часто позволяет избежать избыточных копирований объектов.

Мы хотим передать объект `S s { 1, "alpha" }` в функцию по ссылке, но при этом внутри нее ожидаем от параметра поведения, как будто бы он был передан по значению. Для этого в стандартную библиотеку была введена метафункция `std::decay`. Независимо от того, как был передан в функцию объект обобщенного типа, она преобразует тип в удобную для большинства ситуаций форму.

Уточню. Почти для всех типов `T`метафункция `std::decay<T>` фактически ~~делает харакири~~ удаляет из типа ссылки и const/volatile, как в нашей последней реализации. Исключениями являются функциональные типы и массивы в C-стиле. Для них `std::decay` производит низведение *(decay)* к указателю:
- `int (double)` ➞ `int (*)(double)`;
- `int [3]` ➞ `int *`.

Почему такие странные исключения? Если говорить кратко, виной тому наследие языка C. Это стандартная интерпретация этих типов. Почти в любом контексте компиляторы негласно производят для них низведение. Например, при передаче объектов функциональных типов и C-массивов в функцию по значению

{{<code language="cpp">}}
std::cout
	<< std::boolalpha
	<< std::is_same_v<void (int ()), void (int (*)())> << '\n'
	<< std::is_same_v<void (int [3]), void (int *)> << '\n';

// выведет:
// true
// true
{{</code>}}

На мой взгляд, использование `std::decay` для функций `makeRestorable` и `makeEntity` является более концептуальным и лаконичным. Итоговый вариант реализации функций:

{{<code language="cpp" title="std::decay">}}
template<typename T>
Restorable<std::decay_t<T>> makeRestorable(T&& sample) {
	sample.save();
	return Restorable<std::decay_t<T>>(std::forward<T>(sample));
}

template<typename T>
std::unique_ptr<Entity> makeEntity(T&& sample) {
	return std::make_unique<Restorable<std::decay_t<T>>>(makeRestorable(std::forward<T>(sample)));
}
{{</code>}}

## Дополнительные материалы

[Файл с рабочим исходным кодом (C++17)](tmpl_ref.cpp)

// CodeForces.cpp : Этот файл содержит функцию "main". Здесь начинается и заканчивается выполнение программы.
//

//
#include "pch.h"

#include "multithread.hpp"

#include <fstream>
//enum class Constants {
//	MP = 17, MA = 19, MR = 606
//};
//
//template<typename T>
// constexpr decltype(auto) toUtype(T enumer) noexcept
//{
//	using returnType = typename underlying_type<T>::type;
//	return static_cast<returnType>(enumer);	
//}

using namespace std;


int main()
{	
	vector<int> vec = { 1,2,8,4,5,1,1,1,1,1,1,1,1,1,1,1,1,1,2,1,1,1,1,1,1,1,1,1,3 };
	
	//multithread::partial_sum(begin(vec), end(vec));

	multithread::for_each(begin(vec), end(vec), [](int& s) {++s;});

	cout << "after for_each :  \n";
	for (auto val : vec)
		cout << val << "   ";
	cout << endl;

	auto itr = multithread::find(begin(vec), end(vec), 6);
	
	if (itr == end(vec))
		cout << "Value is not found" << endl;
	else
		cout << "Value found : " << *itr << endl;
	

	system("pause");
	return 0;
}


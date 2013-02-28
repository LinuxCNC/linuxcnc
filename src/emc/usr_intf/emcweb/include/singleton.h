#ifndef XLANG_SINGLETON_H_
#define XLANG_SINGLETON_H_

namespace xlang {

template<typename Derived>
class Singleton {
public:
	static Derived* Instance()
	{
		static Derived the_inst;
		return &the_inst;
	}
protected:
	Singleton()
	{
	}
	~Singleton()
	{
	}
};

}
#endif // XLANG_SINGLETON_H_

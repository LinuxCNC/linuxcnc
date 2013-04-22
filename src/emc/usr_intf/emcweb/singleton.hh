#ifndef SINGLETON_HH_
#define SINGLETON_HH_

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

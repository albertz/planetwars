/*
 *  SimpleBimap.h
 *  PlanetWars bots
 *
 *  Created by Albert Zeyer on 19.09.10.
 *  code under GPLv3
 *
 */

#ifndef __PW__SIMPLEBIMAP_H__
#define __PW__SIMPLEBIMAP_H__

#include <utility>
#include <map>
#include <tr1/memory>

template<typename Parent, typename T, void (Parent::*Setter)(T), T (Parent::*Getter)()>
struct Property {
	Parent& obj;
	Property(Parent& _obj) : obj(_obj) {}
	Property& operator=(T v) { (obj.*Setter)(v); return *this; }
	operator T() { return (obj.*Getter)(); }
};

template<typename T1, typename T2>
class Bimap {
public:
	class Entry;
	typedef std::tr1::shared_ptr<Entry> EntryP;	
	typedef std::multimap<T1, EntryP> T1Map;
	typedef std::multimap<T2, EntryP> T2Map;

	class Entry {
	private:
		Bimap& bimap;
		typename T1Map::iterator it1;
		typename T2Map::iterator it2;
		
		void set1(T1 v) { bimap.map1.erase(it1); it1 = bimap.map1.insert(typename T1Map::value_type(v, it2->second)); }
		void set2(T2 v) { bimap.map2.erase(it2); it2 = bimap.map2.insert(typename T2Map::value_type(v, it1->second)); }
		T1 get1() { return it1->first; }
		T2 get2() { return it2->first; }		
	public:
		Entry(Bimap& _bimap, T1 v1, T2 v2, EntryP& pt) : bimap(_bimap) {
			pt = EntryP(this);
			it1 = bimap.map1.insert(typename T1Map::value_type(v1,pt));
			it2 = bimap.map2.insert(typename T2Map::value_type(v2,pt));		
		}
		Property<Entry,T1,&Entry::set1,&Entry::get1> first() { return Property<Entry,T1,&Entry::set1,&Entry::get1>(*this); }
		Property<Entry,T2,&Entry::set2,&Entry::get2> second() { return Property<Entry,T2,&Entry::set2,&Entry::get2>(*this); }
	};
	
	T1Map map1;
	T2Map map2;
	
	EntryP insert(T1 v1, T2 v2) {
		EntryP entry;
		new Entry(*this, v1, v2, entry);
		return entry;
	}
	
	EntryP find1(T1 v) {
		typename T1Map::iterator i = map1.find(v);
		if(i == map1.end()) return NULL;
		return i->second;
	}

	EntryP find2(T2 v) {
		typename T2Map::iterator i = map2.find(v);
		if(i == map2.end()) return NULL;
		return i->second;
	}
};

#endif
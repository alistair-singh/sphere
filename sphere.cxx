
#include <variant>

namespace sphere {
struct point {
	float x, y, z;
};

struct aabb {
	point mins;
	point maxs;

	static constexpr struct aabb empty() {
		return { { 0.f, 0.f, 0.f }, { 0.f, 0.f, 0.f } };
	}
};

struct otnode {
	aabb regions[8];
	//struct ontnode* children[8];
	//void* data[8];
	std::variant<otnode*, void*> children[8];

	constexpr otnode insert(point p, void* o) {
		return { { }, { } };
	}
};

}

#include <cstdint>
#include <fstream>
#include <vector>
#include <cassert>

template<typename T, size_t S = sizeof(T)>
struct fixed
{
	union {
		T item;
		char raw[S];
	};

	constexpr size_t size() { return S; }
	fixed() : raw{} {
		assert(sizeof(T) < S);
	}
	fixed(T& item_) : raw{}, item(item_) {
		assert(sizeof(T) < S);
	}
	void write(std::ostream &out) {
		out.write(raw, size());
	}
	void read(std::istream &in) {
		in.read(raw, size());
	}
};


#include <array>
template<typename T, size_t S>
struct many {
	std::array<fixed<T>, S> items;
	constexpr size_t size() {return S;}
	void write(std::ostream &out) {
		for (const auto &i : items)
			i.write(out)
	}
	void read(std::istream &in) {
		for (int i =0; i < size(); i++)
		{
			items.emplace_back();
			items[i].item.read(in);
		}
	}
};

template<typename T>
struct var
{
	std::vector<fixed<T>> items;

	constexpr size_t size() {return items.size();}
	void write(std::ostream &out) {
		out.write(size(), sizeof(size_t));
		for (const auto &i : items)
			i.write(out)
	}
	void read(std::istream &in) {
		size_t len = 0;
		in.read(&len, sizeof(size_t));
		assert(len < 1024); // SoftCap
		items.reserve(len);
		for (int i =0; i < len; i++)
		{
			items.emplace_back();
			items[i].item.read(in);
		}
	}
};


typedef size_t address;
struct sheader {
	address next;
	address prev;
	unsigned short size;
};

template <typename T, typename H, size_t S>
struct page {
	typedef H header_t;
	typedef T data_t;
	const static size_t SIZE = S;
	const static size_t CAPACITY = (SIZE - sizeof(header_t))/sizeof(data_t);

	H header;
	T data[CAPACITY];

	T* begin() { return (T*)&data; }
	T* end() { return (T*)&data[header.size]; }
	bool push_back(const T &t) { 
		if(header.size < CAPACITY) {
			data[header.size] = t;
			header.size++;
			return true;
		}	
		return false;
	}
};

template<typename T, size_t S>
using spage = page<T, sheader, S>;

template<typename T>
using spage8k = spage<T, 8192>;

template<typename T, size_t S>
struct vpage {
	typedef T content_t;
	typedef spage<T, S> page_t;
	address addr;
	bool dirty;
	page_t page;
};

template<typename T>
using vpage8k = vpage<T, 8192>;

struct prime {
	char magic[4];
	unsigned int version;
	size_t id;
	address objects = 0x04;
	address free = 0x2004;
};

#include <vector>
class sfile
{
	std::string _path;
	std::fstream _stream;
	fixed<prime, 64> _entry;
	size_t _size;

	sfile(const std::string& path) : _path(path), _entry{} {
		prime().magic[0] = 'A';
		prime().magic[1] = 'L';
		prime().magic[2] = 'I';
		prime().magic[3] = '\0';
		prime().version = 1;
		prime().id = 0;
		prime().objects = 0x40;
		prime().free = 0x2040;
	}

public:
	prime& prime() { return _entry.item; }
	size_t size() { return _size; }

	template<typename T>
	std::vector<address> new_pages(const size_t num = 1) {
		_stream.seekg(0, _stream.end);
		size_t position = _stream.tellg();
		assert(position == _size);

		std::vector<address> addresses;
		addresses.reserve(num);
		for(int i = 0; i < num; ++i) {
			addresses.push_back(position + (spage8k<T>::SIZE * i));
		}

		std::vector<char> data(spage8k<T>::SIZE * num);
		_stream.write(data.data(), spage8k<T>::SIZE * num);
		_size = _stream.tellg();
		return addresses;
	}

	template<typename T>
	void set_page(address adr, const spage8k<T> &blank) {
		_stream.seekg(adr, _stream.beg);
		_stream.write((char*)(&blank), spage8k<T>::SIZE);
	}

	template<typename T>
	spage8k<T> get_page(address adr) {
		if(adr > _size)
			throw "Bad addr";
		spage8k<T> blank;
		_stream.seekg(adr, _stream.beg);
		_stream.read((char*)(&blank), spage8k<T>::SIZE);
		return blank;
	}

	static bool check(const std::string &path) {
		return false;
	}

	static sfile create(const std::string &path) {
		sfile d(path);
		d._stream.open(path, 
				std::ios::in 
				| std::ios::out 
				| std::fstream::trunc
				| std::fstream::binary);
		d._entry.write(d._stream);
		d._size = d._stream.tellg();
		auto addresses = d.new_pages<address>(2);
		d.prime().objects = addresses[0];
		d.prime().free = addresses[1];
		return d;
	}

	static sfile resume(const std::string &path) { 
		sfile d(path);
		d._stream.open(path, 
				std::ios::in 
				| std::ios::out 
				| std::fstream::app
				| std::fstream::binary);

		d._entry.read(d._stream);
		d._stream.seekg(0, d._stream.end);
		d._size = d._stream.tellg();
		return d;
	}
};

#include<unordered_map>
#include<memory>
class page_cache {
	public:
	template<typename T>
	vpage8k<T> *get(address id) {}
	void flush() {}
};

class db {
	public:
	page_cache _cache;
	sfile _file;

	template<typename T>
	std::vector<vpage8k<T>> new_objects(const size_t num = 1) {
		auto addresses = _file.new_pages<T>(num);
		auto vpages = std::vector<vpage8k<T>>();
		vpages.reserve(num);
		for(auto address: addresses) {
			auto page = _file.get_page<T>(address);
			vpages.push_back({ address, false, page });
		}
		return vpages;
	}

	static bool check(const std::string &path) {
		return false;
	}

	static db create(const std::string &path) {
		return { {}, sfile::create(path) };
	}

	static db restore(const std::string &path) { 
		return { {}, sfile::resume(path) };
	}
};

int main() {
	std::vector<sphere::otnode> heap;
	heap.reserve((1024*1024)/sizeof(sphere::otnode));
	std::printf("sphere\n");
	std::printf("sizeof(point):%zu\n", sizeof(sphere::point));
	std::printf("sizeof(aabb):%zu\n", sizeof(sphere::aabb));
	std::printf("sizeof(otnode):%zu\n", sizeof(sphere::otnode));

	std::printf("sizeof(vpage):%zu\n", sizeof(vpage8k<int>));
	std::printf("sizeof(vpage8k::page_t):%zu\n", sizeof(vpage8k<int>::page_t));
	std::printf("vpage8k::page_t::SIZE:%zu\n", vpage8k<int>::page_t::SIZE);
	std::printf("vpage8k::page_t::CAPACITY:%zu\n", vpage8k<int>::page_t::CAPACITY);
	std::printf("vpage8k::page_t::header_t:%zu\n", sizeof(vpage8k<int>::page_t::header_t));
	auto a = sphere::aabb::empty();
	db d = db::create("database.db");
	std::printf("sfile size %zu\n", d._file.size());
	std::printf("sfile objects addr %zu\n", d._file.prime().objects);
	std::printf("sfile free addr %zu\n", d._file.prime().free);
	auto objects = d.new_objects<address>(2);
	std::printf("num of addresses %zu\n", objects.size());
	for(auto object: objects) {
		std::printf("page adr %zu\n", object.page);
		std::printf("page adr %zu\n", object.page);
		//auto vpage = d._file.get_page<address>(object.page);
		auto &page = object.page;

		page.push_back(1);
		page.push_back(4);
		page.push_back(7);
		page.push_back(8);

		std::printf("\tpage next %zu\n", page.header.next);
		std::printf("\tpage prev %zu\n", page.header.prev);
		std::printf("\tpage size %u\n", page.header.size);
		std::printf("\tpage begin %zu\n", page.begin());
		std::printf("\tpage   end %zu\n", page.end());
		int rowId = 0;
		for(auto &row: page) {
			std::printf("\tpage row %i %zi\n", rowId, row);
			rowId++;
		}
		d._file.set_page(object.addr, page);
	}
	std::printf("sfile size %zu\n", d._file.size());
	return 0;
}

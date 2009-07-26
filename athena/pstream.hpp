//
// pstream.hpp
// ~~~~~~~~~~~
//
// Copyright (c) 2009 Rick Yang (rick68 at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef ATHENA_PSTREAM_HPP
#define ATHENA_PSTREAM_HPP

#include <ios>
#include <streambuf>
#include <iostream>

#include <cassert>
#include <cstdio>	// popen()

#if defined(ATHENA_POPEN_STREAM_BUFFER_SIZE) && \
    (ATHENA_POPEN_STREAM_BUFFER_SIZE >= 1)
#  error "ATHENA_POPEN_STREAM_BUFFER_SIZE must more then or equal to 1"
#endif

#ifndef ATHENA_POPEN_STREAM_BUFFER_SIZE
#  define ATHENA_POPEN_STREAM_BUFFER_SIZE	512
#endif

namespace athena {

namespace detail {

    class popenbuf_base
    {
    public:
	popenbuf_base() : c_file_ptr_(0) {}

    public:
	bool popen(const char* command, std::ios_base::openmode mode)
	{
	    openmode_ = mode;
	    const char* mode_str = 0;

#if defined(macintosh) || defined(__APPLE__) || defined(__APPLE_CC__)
	    if (mode == std::ios_base::in | std::ios_base::out)
		mode_str = "r+";
	    else
#endif
	    if (mode == std::ios_base::in)
		mode_str = "r";
	    else if (mode == std::ios_base::out)
		mode_str = "w";
	    else
		assert(false);

	    c_file_ptr_ = ::popen(command, mode_str);
	    assert(c_file_ptr_);
	    return c_file_ptr_ != 0;
	}

	bool pclose()
	{
	    bool ok = is_open();
	    if (ok)
	    {
		ok &= ::pclose(c_file_ptr_) != -1;
		assert(ok);
		c_file_ptr_ = 0;
	    }
	    return ok;
	}

    public:
	std::ptrdiff_t read(char* buf, std::ptrdiff_t n)
	{
	    assert(is_open());
	    if (feof(c_file_ptr_))
		return -1;
	    return ::fread(buf, 1, n, c_file_ptr_);
	}

	bool write(char* buf, std::ptrdiff_t n)
	{
    	    assert(is_open());
	    if (feof(c_file_ptr_))
		return false;
	    return ::fwrite(buf, 1, n, c_file_ptr_);
	}

	bool flush()
	{
	    assert(is_open());
	    assert(openmode_ & std::ios_base::out);
	    return !::fflush(c_file_ptr_);
	}

    public:
	bool is_open() const
	    { return c_file_ptr_ != 0; }

	int mode() const
	    { return static_cast<int>(openmode_); }

    protected:
	std::ios_base::openmode openmode_;
	::FILE* c_file_ptr_;
    };

} // namespace detail


template <typename CharT, typename Traits>
class basic_popenbuf : public std::basic_streambuf<CharT, Traits>
{
public:
    typedef CharT			char_type;
    typedef typename Traits::int_type	int_type;
    typedef typename Traits::pos_type	pos_type;
    typedef typename Traits::off_type	off_type;
    typedef Traits			traits_type;

    typedef std::basic_streambuf<CharT, Traits> _Base;
    typedef basic_popenbuf<CharT, Traits>	_Self;

public:
    basic_popenbuf()
      : base_()
      , total_buff_(new char_type[ATHENA_POPEN_STREAM_BUFFER_SIZE])
      , total_buff_size_(ATHENA_POPEN_STREAM_BUFFER_SIZE)
      , in_buff_(0)
      , in_buff_size_(0)
      , out_buff_(0)
      , out_buff_size_(0)
      , user_custom_buffer_(false)
    {}

    ~basic_popenbuf()
    {
	if (!user_custom_buffer_)
	    delete [] total_buff_;
	close();
    }

public:
    bool is_open() const { return base_.is_open(); }

    _Self* open(const char* command,
		std::ios_base::openmode mode = std::ios_base::in)
    {
	assert(!is_open());
	assert(!_Base::eback());
	assert(!_Base::gptr());
	assert(!_Base::egptr());
	assert(!_Base::pbase());
	assert(!_Base::pptr());
	assert(!_Base::epptr());
	assert(!in_buff_);
	assert(!out_buff_);
	assert(!in_buff_size_);
	assert(!out_buff_size_);

	bool ok = base_.popen(command, mode);

#if defined(macintosh) || defined(__APPLE__) || defined(__APPLE_CC__)
	if (mode == std::ios_base::in | std::ios_base::out)
	{
	    in_buff_ = total_buff_;
	    in_buff_size_ = (total_buff_size_ + 1) / 2;
	    out_buff_ = in_buff_ + in_buff_size_;
	    out_buff_size_ = total_buff_size_ - in_buff_size_;
	    _Base::setg(in_buff_, in_buff_, in_buff_);
	    _Base::setp(out_buff_, out_buff_ + out_buff_size_);
	}
	else
#endif
	if (mode == std::ios_base::in)
	{
	    in_buff_ = total_buff_;
	    in_buff_size_ = total_buff_size_;
	    _Base::setg(in_buff_, in_buff_, in_buff_);
	}
	else if (mode == std::ios_base::out)
	{
	    out_buff_ = total_buff_;
	    out_buff_size_ = total_buff_size_;
	    _Base::setp(out_buff_, out_buff_ + out_buff_size_);
	}
	else
	    assert(false);

	return ok ? this : 0;
    }

    _Self* close()
    {
	bool ok = is_open();

	if (base_.mode() & std::ios_base::out)
	    sync();

	ok &= base_.pclose();

	if (ok)
	{
	    _Base::setg(0, 0, 0);
	    _Base::setp(0, 0);
	    in_buff_ = out_buff_ = 0;
	    in_buff_size_ = out_buff_size_ = 0;
	    user_custom_buffer_ = false;
	}

	return ok ? this : 0;
    }

protected:
    virtual int_type underflow()
    {
	std::streamsize n;
	if ((n = base_.read(_Base::eback(), in_buff_size_)) == -1)
	{
	    _Base::setg(0, 0, 0);
	    return traits_type::eof();
	}

	_Base::setg(_Base::eback(), _Base::eback(), _Base::eback() + n);
	return traits_type::to_int_type(*_Base::gptr());
    }

    virtual int_type overflow(int_type c = traits_type::eof())
    {
	if (traits_type::eq(c, traits_type::eof()))
	{
	    sync();
	    return traits_type::eof();
	}

	if (!out_buff_size_)
	{
	    char_type c_tmp = traits_type::to_char_type(c);
	    if (!base_.write(&c_tmp, 1))
		return traits_type::eof();
	}
	else
	{
	    if (!base_.write(_Base::pbase(), out_buff_size_))
		return traits_type::eof();
	    *_Base::pbase() = c;
	    _Base::pbump(-out_buff_size_ + 1);
	}

	return traits_type::not_eof(c);
    }

    virtual int sync()
    {
	if (base_.mode() & std::ios_base::out)
	{
	    std::streamsize n = _Base::pptr() - _Base::pbase();
	    bool ok = base_.write(_Base::pbase(), n);
	    _Base::pbump(-n);
	    ok &= base_.flush();
	    return ok ? 0 : -1;
	}

	return 0;
    }


    virtual _Base* setbuf(char_type* buf, std::streamsize n)
    {
	assert(is_open());

	if (is_open()) return 0;

	if (buf == 0)
	{
	    if (!user_custom_buffer_)
		return this;
	    user_custom_buffer_ = false;
	    total_buff_ = new char_type[ATHENA_POPEN_STREAM_BUFFER_SIZE];
	    total_buff_size_ = ATHENA_POPEN_STREAM_BUFFER_SIZE;
	    return this;
	}

	user_custom_buffer_ = true;
	delete [] total_buff_;
	total_buff_ = buf;
	total_buff_size_ = n;

	return this;
    }

private:
    detail::popenbuf_base base_;
    char_type* total_buff_;
    std::size_t total_buff_size_;
    char_type* in_buff_;
    std::size_t in_buff_size_;
    char_type* out_buff_;
    std::size_t out_buff_size_;
    bool user_custom_buffer_;
};


template <typename CharT, typename Traits>
class basic_ipstream : public std::basic_istream<CharT, Traits>
{
public:
    typedef CharT			char_type;
    typedef typename Traits::int_type	int_type;
    typedef typename Traits::pos_type	pos_type;
    typedef typename Traits::off_type	off_type;
    typedef Traits			traits_type;

    typedef std::basic_ios<CharT, Traits>	_Basic_ios;
    typedef std::basic_istream<CharT, Traits>	_Base;
    typedef basic_popenbuf<CharT, Traits>	_Buf;

public:
    basic_ipstream() : _Basic_ios(), _Base(0), buf_()
      { _Base::init(&buf_); }

    explicit basic_ipstream(const char* commond,
			    std::ios_base::openmode mode = std::ios_base::in)
      : _Basic_ios(), _Base(0), buf_()
    {
	_Base::init(&buf_);
	open(commond, mode);
    }

    ~basic_ipstream() {}

public:
    _Buf* rdbuf() const
      { return const_cast<_Buf*>(&buf_); }

    bool is_open()
      { return buf_.is_open(); }

    void open(const char* commond,
	      std::ios_base::openmode mode = std::ios_base::in)
    {
	if (!buf_.open(commond, mode | std::ios_base::in))
	    _Basic_ios::setstate(std::ios_base::failbit);
    }

    void close()
    {
	if (!buf_.close())
	    _Basic_ios::setstate(std::ios_base::failbit);
    }

private:
    _Buf buf_;
};


template <typename CharT, typename Traits>
class basic_opstream : public std::basic_ostream<CharT, Traits>
{
public:
    typedef CharT			char_type;
    typedef typename Traits::int_type	int_type;
    typedef typename Traits::pos_type	pos_type;
    typedef typename Traits::off_type	off_type;
    typedef Traits			traits_type;

    typedef std::basic_ios<CharT, Traits>	_Basic_ios;
    typedef std::basic_ostream<CharT, Traits>	_Base;
    typedef basic_popenbuf<CharT, Traits>	_Buf;

public:
    basic_opstream() : _Basic_ios(), _Base(0), buf_()
      { _Base::init(&buf_); }

    explicit basic_opstream(const char* commond,
			    std::ios_base::openmode mode = std::ios_base::out)
      : _Basic_ios(), _Base(0), buf_()
    {
	_Base::init(&buf_);
	open(commond, mode);
    }

    ~basic_opstream() {}

public:
    _Buf* rdbuf() const
      { return const_cast<_Buf*>(&buf_); }

    bool is_open()
      { return buf_.is_open(); }

    void open(const char* commond,
	      std::ios_base::openmode mode = std::ios_base::out)
    {
	if (!buf_.open(commond, mode | std::ios_base::out))
	    _Basic_ios::setstate(std::ios_base::failbit);
    }

    void close()
    {
	if (!buf_.close())
	    _Basic_ios::setstate(std::ios_base::failbit);
    }

private:
    _Buf buf_;
};


#if defined(macintosh) || defined(__APPLE__) || defined(__APPLE_CC__)
template <typename CharT, typename Traits>
class basic_pstream : public std::basic_iostream<CharT, Traits>
{
public:
    typedef CharT			char_type;
    typedef typename Traits::int_type	int_type;
    typedef typename Traits::pos_type	pos_type;
    typedef typename Traits::off_type	off_type;
    typedef Traits			traits_type;

    typedef std::basic_ios<CharT, Traits>	_Basic_ios;
    typedef std::basic_iostream<CharT, Traits>	_Base;
    typedef basic_popenbuf<CharT, Traits>	_Buf;

public:
    basic_pstream() : _Basic_ios(), _Base(0), buf_()
      { _Base::init(&buf_); }

    explicit basic_pstream(const char* commond
	, std::ios_base::openmode mode = std::ios_base::in | std::ios_base::out)
      : _Basic_ios(), _Base(0), buf_()
    {
	_Base::init(&buf_);
	open(commond, mode);
    }

    ~basic_pstream() {}

public:
    _Buf* rdbuf() const
      { return const_cast<_Buf*>(&buf_); }

    bool is_open()
      { return buf_.is_open(); }

    void open(const char* commond
	, std::ios_base::openmode mode = std::ios_base::in | std::ios_base::out)
    {
	if (!buf_.open(commond, mode | std::ios_base::out))
	    _Basic_ios::setstate(std::ios_base::failbit);
    }

    void close()
    {
	if (!buf_.close())
	    _Basic_ios::setstate(std::ios_base::failbit);
    }

private:
    _Buf buf_;
    typedef basic_pstream<CharT, Traits> _Self;
    basic_pstream(_Self const&);
    _Self& operator=(_Self const&);
};
#ndif

typedef basic_ipstream<char, std::char_traits<char> > ipstream;
typedef basic_opstream<char, std::char_traits<char> > opstream;

typedef basic_ipstream<wchar_t, std::char_traits<wchar_t> > wipstream;
typedef basic_opstream<wchar_t, std::char_traits<wchar_t> > wopstream;

#if defined(macintosh) || defined(__APPLE__) || defined(__APPLE_CC__)
typedef basic_pstream<char, std::char_traits<char> > pstream;
typedef basic_pstream<wchar_t, std::char_traits<wchar_t> > wpstream;
#endif

} // namespace athena

#endif // ATHENA_PSTREAM_HPP

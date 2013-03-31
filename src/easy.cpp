
#include <boost/make_shared.hpp>
#include <curl-asio/easy.h>
#include <curl-asio/error_code.h>
#include <curl-asio/form.h>
#include <curl-asio/multi.h>
#include <curl-asio/share.h>
#include <curl-asio/string_list.h>

using namespace curl;

easy* easy::from_native(native::CURL* native_easy)
{
	easy* easy_handle;
	native::curl_easy_getinfo(native_easy, native::CURLINFO_PRIVATE, &easy_handle);
	return easy_handle;
}

easy::easy(boost::asio::io_service& io_service):
	io_service_(io_service),
	multi_(0),
	multi_registered_(false)
{
	init();
}

easy::easy(multi& multi_handle):
	io_service_(multi_handle.get_io_service()),
	multi_(&multi_handle),
	multi_registered_(false)
{
	init();
}

easy::~easy()
{
	// ensure to cancel all async. transfers when this object is destroyed
	if (multi_registered_)
	{
		multi_->remove(this);
		multi_registered_ = false;
	}

	if (handle_)
	{
		native::curl_easy_cleanup(handle_);
		handle_ = 0;
	}
}

void easy::perform()
{
	boost::system::error_code ec(native::curl_easy_perform(handle_));
	boost::asio::detail::throw_error(ec);
}

void easy::async_perform(handler_type handler)
{
	if (!multi_)
	{
		throw std::runtime_error("attempt to perform async. operation without assigning a multi object");
	}

	cancel();
	multi_->add(this, handler_wrapper(this, handler));
	multi_registered_ = true;
}

void easy::async_perform(multi& multi_handle, handler_type handler)
{
	cancel();
	multi_ = &multi_handle;
	multi_->add(this, handler_wrapper(this, handler));
	multi_registered_ = true;
}

void easy::cancel()
{
	if (multi_registered_)
	{
		multi_->remove(this);
		multi_registered_ = false;
	}
}

void easy::set_source(boost::shared_ptr<std::istream> source)
{
	source_ = source;
	set_read_function(&easy::read_function);
	set_read_data(this);
	set_seek_function(&easy::seek_function);
	set_seek_data(this);
}

void easy::set_sink(boost::shared_ptr<std::ostream> sink)
{
	sink_ = sink;
	set_write_function(&easy::write_function);
	set_write_data(this);
}

easy::socket_type* easy::get_socket_from_native(native::curl_socket_t native_socket)
{
	socket_map_type::iterator it = sockets_.find(native_socket);

	if (it != sockets_.end())
	{
		return it->second;
	}
	else
	{
		return 0;
	}
}

void easy::set_post_fields(const std::string& post_fields)
{
	post_fields_ = post_fields;
	boost::system::error_code ec(native::curl_easy_setopt(handle_, native::CURLOPT_POSTFIELDS, post_fields_.c_str()));
	boost::asio::detail::throw_error(ec);
	set_post_field_size_large(static_cast<native::curl_off_t>(post_fields_.length()));
}

void easy::set_http_post(boost::shared_ptr<form> form)
{
	form_ = form;

	if (form_)
	{
		boost::system::error_code ec(native::curl_easy_setopt(handle_, native::CURLOPT_HTTPPOST, form_->native_handle()));
		boost::asio::detail::throw_error(ec);
	}
	else
	{
		boost::system::error_code ec(native::curl_easy_setopt(handle_, native::CURLOPT_HTTPPOST, NULL));
		boost::asio::detail::throw_error(ec);
	}
}

void easy::add_header(const std::string& name, const std::string& value)
{
	add_header(name + ": " + value);
}

void easy::add_header(const std::string& header)
{
	if (!headers_)
	{
		headers_ = boost::make_shared<string_list>();
	}

	headers_->add(header);

	boost::system::error_code ec(native::curl_easy_setopt(handle_, native::CURLOPT_HTTPHEADER, headers_->native_handle()));
	boost::asio::detail::throw_error(ec);
}

void easy::set_headers(boost::shared_ptr<string_list> headers)
{
	headers_ = headers;

	if (headers_)
	{
		boost::system::error_code ec(native::curl_easy_setopt(handle_, native::CURLOPT_HTTPHEADER, headers_->native_handle()));
		boost::asio::detail::throw_error(ec);
	}
	else
	{
		boost::system::error_code ec(native::curl_easy_setopt(handle_, native::CURLOPT_HTTPHEADER, NULL));
		boost::asio::detail::throw_error(ec);
	}
}

void easy::add_http200_alias(const std::string& http200_alias)
{
	if (!http200_aliases_)
	{
		http200_aliases_ = boost::make_shared<string_list>();
	}

	http200_aliases_->add(http200_alias);

	boost::system::error_code ec(native::curl_easy_setopt(handle_, native::CURLOPT_HTTP200ALIASES, http200_aliases_->native_handle()));
	boost::asio::detail::throw_error(ec);
}

void easy::set_http200_aliases(boost::shared_ptr<string_list> http200_aliases)
{
	http200_aliases_ = http200_aliases;

	if (http200_aliases)
	{
		boost::system::error_code ec(native::curl_easy_setopt(handle_, native::CURLOPT_HTTP200ALIASES, http200_aliases->native_handle()));
		boost::asio::detail::throw_error(ec);
	}
	else
	{
		boost::system::error_code ec(native::curl_easy_setopt(handle_, native::CURLOPT_HTTP200ALIASES, NULL));
		boost::asio::detail::throw_error(ec);
	}
}

void easy::add_mail_rcpt(const std::string& mail_rcpt)
{
	if (!mail_rcpts_)
	{
		mail_rcpts_ = boost::make_shared<string_list>();
	}

	mail_rcpts_->add(mail_rcpt);

	boost::system::error_code ec(native::curl_easy_setopt(handle_, native::CURLOPT_MAIL_RCPT, mail_rcpts_->native_handle()));
	boost::asio::detail::throw_error(ec);
}

void easy::set_mail_rcpts(boost::shared_ptr<string_list> mail_rcpts)
{
	mail_rcpts_ = mail_rcpts;

	if (mail_rcpts_)
	{
		boost::system::error_code ec(native::curl_easy_setopt(handle_, native::CURLOPT_MAIL_RCPT, mail_rcpts_->native_handle()));
		boost::asio::detail::throw_error(ec);
	}
	else
	{
		boost::system::error_code ec(native::curl_easy_setopt(handle_, native::CURLOPT_MAIL_RCPT, NULL));
		boost::asio::detail::throw_error(ec);
	}
}

void easy::add_quote(const std::string& quote)
{
	if (!quotes_)
	{
		quotes_ = boost::make_shared<string_list>();
	}

	quotes_->add(quote);

	boost::system::error_code ec(native::curl_easy_setopt(handle_, native::CURLOPT_QUOTE, quotes_->native_handle()));
	boost::asio::detail::throw_error(ec);
}

void easy::set_quotes(boost::shared_ptr<string_list> quotes)
{
	quotes_ = quotes;

	if (mail_rcpts_)
	{
		boost::system::error_code ec(native::curl_easy_setopt(handle_, native::CURLOPT_QUOTE, quotes_->native_handle()));
		boost::asio::detail::throw_error(ec);
	}
	else
	{
		boost::system::error_code ec(native::curl_easy_setopt(handle_, native::CURLOPT_QUOTE, NULL));
		boost::asio::detail::throw_error(ec);
	}
}

void easy::add_resolve(const std::string& resolved_host)
{
	if (!resolved_hosts_)
	{
		resolved_hosts_ = boost::make_shared<string_list>();
	}

	resolved_hosts_->add(resolved_host);

	boost::system::error_code ec(native::curl_easy_setopt(handle_, native::CURLOPT_RESOLVE, resolved_hosts_->native_handle()));
	boost::asio::detail::throw_error(ec);
}

void easy::set_resolves(boost::shared_ptr<string_list> resolved_hosts)
{
	resolved_hosts_ = resolved_hosts;

	if (resolved_hosts_)
	{
		boost::system::error_code ec(native::curl_easy_setopt(handle_, native::CURLOPT_RESOLVE, resolved_hosts_->native_handle()));
		boost::asio::detail::throw_error(ec);
	}
	else
	{
		boost::system::error_code ec(native::curl_easy_setopt(handle_, native::CURLOPT_RESOLVE, NULL));
		boost::asio::detail::throw_error(ec);
	}
}

void easy::set_share(boost::shared_ptr<share> share)
{
	share_ = share;

	if (share)
	{
		boost::system::error_code ec(native::curl_easy_setopt(handle_, native::CURLOPT_SHARE, share->native_handle()));
		boost::asio::detail::throw_error(ec);
	}
	else
	{
		boost::system::error_code ec(native::curl_easy_setopt(handle_, native::CURLOPT_SHARE, NULL));
		boost::asio::detail::throw_error(ec);
	}
}

void easy::add_telnet_option(const std::string& telnet_option)
{
	if (!telnet_options_)
	{
		telnet_options_ = boost::make_shared<string_list>();
	}

	telnet_options_->add(telnet_option);

	boost::system::error_code ec(native::curl_easy_setopt(handle_, native::CURLOPT_TELNETOPTIONS, telnet_options_->native_handle()));
	boost::asio::detail::throw_error(ec);
}

void easy::add_telnet_option(const std::string& option, const std::string& value)
{
	add_telnet_option(option + "=" + value);
}

void easy::set_telnet_options(boost::shared_ptr<string_list> telnet_options)
{
	telnet_options_ = telnet_options;

	if (telnet_options_)
	{
		boost::system::error_code ec(native::curl_easy_setopt(handle_, native::CURLOPT_TELNETOPTIONS, telnet_options_->native_handle()));
		boost::asio::detail::throw_error(ec);
	}
	else
	{
		boost::system::error_code ec(native::curl_easy_setopt(handle_, native::CURLOPT_TELNETOPTIONS, NULL));
		boost::asio::detail::throw_error(ec);
	}
}

void easy::init()
{
	handle_ = native::curl_easy_init();

	if (!handle_)
	{
		throw std::bad_alloc();
	}

	set_private(this);
	set_opensocket_function(&easy::opensocket);
	set_opensocket_data(this);
	set_closesocket_function(&easy::closesocket);
	set_closesocket_data(this);
}

size_t easy::write_function(char* ptr, size_t size, size_t nmemb, void* userdata)
{
	easy* self = static_cast<easy*>(userdata);
	size_t actual_size = size * nmemb;

	if (!actual_size)
	{
		return 0;
	}

	if (!self->sink_->write(ptr, actual_size))
	{
		return 0;
	}
	else
	{
		return actual_size;
	}
}

size_t easy::read_function(void* ptr, size_t size, size_t nmemb, void* userdata)
{
	// FIXME readsome doesn't work with TFTP (see cURL docs)

	easy* self = static_cast<easy*>(userdata);
	size_t actual_size = size * nmemb;

	if (self->source_->eof())
	{
		return 0;
	}

	std::streamsize chars_stored = self->source_->readsome(static_cast<char*>(ptr), actual_size);

	if (!self->source_)
	{
		return CURL_READFUNC_ABORT;
	}
	else
	{
		return static_cast<size_t>(chars_stored);
	}
}

int easy::seek_function(void* instream, native::curl_off_t offset, int origin)
{
	// TODO we could allow the user to define an offset which this library should consider as position zero for uploading chunks of the file
	// alternatively do tellg() on the source stream when it is first passed to use_source() and use that as origin

	easy* self = static_cast<easy*>(instream);

	std::ios::seek_dir way;

	switch (origin)
	{
	case SEEK_SET:
		way = std::ios::beg;
		break;

	case SEEK_CUR:
		way = std::ios::cur;
		break;

	case SEEK_END:
		way = std::ios::end;
		break;

	default:
		return CURL_SEEKFUNC_FAIL;
	}

	if (!self->source_->seekg(offset, way))
	{
		return CURL_SEEKFUNC_FAIL;
	}
	else
	{
		return CURL_SEEKFUNC_OK;
	}
}

native::curl_socket_t easy::opensocket(void* clientp, native::curlsocktype purpose, struct native::curl_sockaddr* address)
{
	easy* self = static_cast<easy*>(clientp);

	if (purpose == native::CURLSOCKTYPE_IPCXN)
	{
		boost::system::error_code ec;
		std::auto_ptr<socket_type> socket(new socket_type(self->io_service_));

		switch (address->family)
		{
		case AF_INET:
			socket->open(boost::asio::ip::tcp::v4(), ec);
			break;

		case AF_INET6:
			socket->open(boost::asio::ip::tcp::v6(), ec);
			break;
		}

		if (ec)
		{
			return CURL_SOCKET_BAD;
		}
		else
		{
			socket_type::native_handle_type fd = socket->native_handle();
			self->sockets_.insert(fd, socket);
			return fd;
		}
	}

	return CURL_SOCKET_BAD;
}

int easy::closesocket(void* clientp, native::curl_socket_t item)
{
	easy* self = static_cast<easy*>(clientp);

	socket_map_type::iterator it = self->sockets_.find(item);

	if (it != self->sockets_.end())
	{
		self->sockets_.erase(it);
	}

	return 0;
}

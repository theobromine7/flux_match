#include <boost/beast/core.hpp>
#include <boost/beast/websocket.hpp>
#include <boost/beast/ssl.hpp>
#include <boost/json.hpp>
#include <boost/beast/http.hpp>
#include <boost/asio/connect.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <boost/asio/ssl.hpp>

#include <MarketData.hpp>
#include <MarketDepth.hpp>

#include <iostream>
#include <string>
#include <stdexcept>

long long parseFixed(const std::string &str, long long scale);
fluxmatch::DepthSnapshot getSnapshot(
    const fluxmatch::Instrument &instrument);

fluxmatch::DepthUpdate parseDepthUpdate(
    const std::string &data,
    const fluxmatch::Instrument &instrument);

int main()
{
    namespace beast = boost::beast;
    namespace websocket = beast::websocket;
    namespace net = boost::asio;

    fluxmatch::Instrument btcusdt{
        100000000,
        100000000};

    fluxmatch::MarketDepth depth;
    auto snapshot = getSnapshot(btcusdt);

    std::cout
        << "Snapshot update ID: "
        << snapshot.lastUpdateId
        << '\n';

    std::cout
        << "Snapshot bids: "
        << snapshot.bids.size()
        << '\n';

    std::cout
        << "Snapshot asks: "
        << snapshot.asks.size()
        << '\n';

    try
    {
        net::io_context ioc;

        net::ssl::context ctx(
            net::ssl::context::tls_client);

        net::ip::tcp::resolver resolver(ioc);

        websocket::stream<
            net::ssl::stream<beast::tcp_stream>>
            ws(ioc, ctx);

        std::cout << "Resolving...\n";

        auto const results =
            resolver.resolve(
                "stream.binance.com",
                "443");

        std::cout << "Connecting...\n";

        beast::get_lowest_layer(ws).connect(results);

        std::cout << "TCP connected!\n";

        std::cout << "Performing TLS handshake...\n";

        ws.next_layer().handshake(
            net::ssl::stream_base::client);

        std::cout << "TLS connected!\n";

        std::cout << "Performing WebSocket handshake...\n";

        ws.handshake(
            "stream.binance.com",
            "/ws/btcusdt@depth@100ms");

        std::cout << "WebSocket connected!\n";

        while (true)
        {
            beast::flat_buffer buffer;

            ws.read(buffer);

            std::string data =
                beast::buffers_to_string(buffer.data());

            auto update =
                parseDepthUpdate(data, btcusdt);

            depth.apply(update);
            depth.printTop();
        }
    }
    catch (const beast::system_error &e)
    {
        std::cerr
            << "Beast error: "
            << e.code()
            << " - "
            << e.what()
            << '\n';
    }
    catch (const std::exception &e)
    {
        std::cerr
            << "Error: "
            << e.what()
            << '\n';
    }

    return 0;
}

fluxmatch::DepthUpdate parseDepthUpdate(
    const std::string &data,
    const fluxmatch::Instrument &instrument)
{
    auto value = boost::json::parse(data);
    auto obj = value.as_object();

    fluxmatch::DepthUpdate update;

    update.firstUpdateId =
        obj.at("U").as_int64();

    update.lastUpdateId =
        obj.at("u").as_int64();

    for (const auto &x : obj.at("b").as_array())
    {
        auto level = x.as_array();

        long long price =
            parseFixed(
                std::string(level[0].as_string()),
                instrument.priceScale);

        long long quantity =
            parseFixed(
                std::string(level[1].as_string()),
                instrument.quantityScale);

        update.bids.push_back({price, quantity});
    }

    for (const auto &x : obj.at("a").as_array())
    {
        auto level = x.as_array();

        long long price =
            parseFixed(
                std::string(level[0].as_string()),
                instrument.priceScale);

        long long quantity =
            parseFixed(
                std::string(level[1].as_string()),
                instrument.quantityScale);

        update.asks.push_back({price, quantity});
    }

    return update;
}

long long parseFixed(
    const std::string &str,
    long long scale)
{
    int n = str.size();

    auto pos = str.find('.');

    int digits = 0;
    long long temp = scale;

    while (temp > 1)
    {
        temp /= 10;
        digits++;
    }

    if (pos == std::string::npos)
    {
        long long ans = std::stoll(str);
        return ans * scale;
    }

    int decimal = n - pos - 1;

    if (decimal > digits)
        throw std::invalid_argument(
            "Too many decimal places");

    long long ans =
        std::stoll(str.substr(0, pos));

    std::string decimalPart =
        str.substr(pos + 1);

    long long decimalValue =
        decimalPart.empty()
            ? 0
            : std::stoll(decimalPart);

    for (int i = decimal; i < digits; i++)
        decimalValue *= 10;

    return ans * scale + decimalValue;
}
fluxmatch::DepthSnapshot getSnapshot(
    const fluxmatch::Instrument &instrument)
{
    namespace beast = boost::beast;
    namespace http = beast::http;
    namespace net = boost::asio;

    net::io_context ioc;

    net::ssl::context ctx(
        net::ssl::context::tls_client);

    net::ip::tcp::resolver resolver(ioc);

    beast::ssl_stream<
        beast::tcp_stream>
        stream(ioc, ctx);

    auto const results =
        resolver.resolve(
            "api.binance.com",
            "443");

    beast::get_lowest_layer(stream).connect(results);

    stream.handshake(
        net::ssl::stream_base::client);

    http::request<http::string_body> req(
        http::verb::get,
        "/api/v3/depth?symbol=BTCUSDT&limit=5000",
        11);

    req.set(
        http::field::host,
        "api.binance.com");

    req.set(
        http::field::user_agent,
        "FluxMatch");

    http::write(stream, req);

    beast::flat_buffer buffer;

    http::response<http::string_body> res;

    http::read(
        stream,
        buffer,
        res);

    if (res.result() != http::status::ok)
    {
        throw std::runtime_error(
            "Snapshot request failed");
    }

    auto value =
        boost::json::parse(res.body());

    auto obj = value.as_object();

    fluxmatch::DepthSnapshot snapshot;

    snapshot.lastUpdateId =
        obj.at("lastUpdateId").as_int64();

    for (const auto &x :
         obj.at("bids").as_array())
    {
        auto level = x.as_array();

        long long price =
            parseFixed(
                std::string(level[0].as_string()),
                instrument.priceScale);

        long long quantity =
            parseFixed(
                std::string(level[1].as_string()),
                instrument.quantityScale);

        snapshot.bids.push_back(
            {price, quantity});
    }

    for (const auto &x :
         obj.at("asks").as_array())
    {
        auto level = x.as_array();

        long long price =
            parseFixed(
                std::string(level[0].as_string()),
                instrument.priceScale);

        long long quantity =
            parseFixed(
                std::string(level[1].as_string()),
                instrument.quantityScale);

        snapshot.asks.push_back(
            {price, quantity});
    }

    beast::error_code ec;

    stream.shutdown(ec);

    return snapshot;
}
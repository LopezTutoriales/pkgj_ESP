#include "vitahttp.hpp"

#include <psp2/io/fcntl.h>
#include <psp2/net/http.h>
#include <psp2/net/net.h>
#include <psp2/net/netctl.h>

#include <fmt/format.h>

#include <boost/scope_exit.hpp>

#define PKGI_USER_AGENT "libhttp/3.65 (PS Vita)"

struct pkgi_http
{
    int used;

    int tmpl;
    int conn;
    int req;
};

namespace
{
static pkgi_http g_http[4];
}

VitaHttp::~VitaHttp()
{
    if (_http)
    {
        LOG("http cerrado");
        sceHttpDeleteRequest(_http->req);
        sceHttpDeleteConnection(_http->conn);
        sceHttpDeleteTemplate(_http->tmpl);
        _http->used = 0;
    }
}

void VitaHttp::start(const std::string& url, uint64_t offset)
{
    if (_http)
        throw HttpError("Conexion HTTP ya iniciada");

    LOG("obtener http");

    pkgi_http* http = NULL;
    for (size_t i = 0; i < 4; i++)
    {
        if (g_http[i].used == 0)
        {
            http = g_http + i;
            break;
        }
    }

    if (!http)
        throw HttpError("error interno: muchas solicitudes http simultaneas");

    int tmpl = -1;
    int conn = -1;
    int req = -1;

    LOGF("iniciando solicitud http GET para {}", url);

    if ((tmpl = sceHttpCreateTemplate(
                 PKGI_USER_AGENT, SCE_HTTP_VERSION_1_1, SCE_TRUE)) < 0)
        throw HttpError(fmt::format(
                "Fallo sceHttpCreateTemplate: {:#08x}",
                static_cast<uint32_t>(tmpl)));
    BOOST_SCOPE_EXIT_ALL(&)
    {
        if (tmpl > 0)
            sceHttpDeleteTemplate(tmpl);
    };
    // sceHttpSetRecvTimeOut(tmpl, 10 * 1000 * 1000);

    if ((conn = sceHttpCreateConnectionWithURL(tmpl, url.c_str(), SCE_FALSE)) <
        0)
        throw HttpError(fmt::format(
                "Fallo sceHttpCreateConnectionWithURL: {:#08x}",
                static_cast<uint32_t>(conn)));
    BOOST_SCOPE_EXIT_ALL(&)
    {
        if (conn > 0)
            sceHttpDeleteConnection(conn);
    };

    if ((req = sceHttpCreateRequestWithURL(
                 conn, SCE_HTTP_METHOD_GET, url.c_str(), 0)) < 0)
        throw HttpError(fmt::format(
                "Fallo sceHttpCreateRequestWithURL: {:#08x}",
                static_cast<uint32_t>(req)));
    BOOST_SCOPE_EXIT_ALL(&)
    {
        if (req > 0)
            sceHttpDeleteRequest(req);
    };

    int err;

    if (offset != 0)
    {
        char range[64];
        pkgi_snprintf(range, sizeof(range), "bytes=%llu-", offset);
        if ((err = sceHttpAddRequestHeader(
                     req, "Rango", range, SCE_HTTP_HEADER_ADD)) < 0)
            throw HttpError(fmt::format(
                    "Fallo sceHttpAddRequestHeader: {:#08x}",
                    static_cast<uint32_t>(err)));
    }

    if ((err = sceHttpSendRequest(req, NULL, 0)) < 0)
    {
        std::string err_msg;
        switch (static_cast<uint32_t>(err))
        {
        case 0x80431063:
            err_msg = "Error de red";
            break;
        case 0x80431068:
            err_msg = "Red - Tiempo agotado";
            break;
        case 0x80431082:
            err_msg = "Solicitud bloqueada";
            break;
        case 0x80436007:
            err_msg = "Host no encontrado";
            break;
        case 0x80431084:
            err_msg = "Error de Proxy";
            break;
        case 0x80431075:
            err_msg = "Error de SSL";
            break;
        default:
            err_msg = "";
        }

        throw formatEx<HttpError>(
                "Fallo sceHttpSendRequest: {:#08x}\n{}",
                static_cast<uint32_t>(err),
                err_msg);
    }

    http->used = 1;
    http->tmpl = tmpl;
    http->conn = conn;
    http->req = req;
    tmpl = conn = req = -1;

    _http = http;
}

int64_t VitaHttp::read(uint8_t* buffer, uint64_t size)
{
    check_status();

    int read = sceHttpReadData(_http->req, buffer, size);
    if (read < 0)
        throw HttpError(fmt::format(
                "Error descarga HTTP {:#08x}",
                static_cast<uint32_t>(static_cast<int32_t>(read))));
    return read;
}

void VitaHttp::abort()
{
    if (_http)
    {
        const auto err = sceHttpAbortRequest(_http->req);
        if (err)
            LOGF("abortar() fallo: {:#08x}", static_cast<uint32_t>(err));
    }
}

int64_t VitaHttp::get_length()
{
    check_status();

    int res;
    uint64_t content_length;
    res = sceHttpGetResponseContentLength(_http->req, &content_length);
    if (res < 0)
        throw HttpError(fmt::format(
                "Fallo sceHttpGetResponseContentLength: {:#08x}",
                static_cast<uint32_t>(res)));
    if (res == (int)SCE_HTTP_ERROR_NO_CONTENT_LENGTH ||
        res == (int)SCE_HTTP_ERROR_CHUNK_ENC)
    {
        LOG("respuesta http sin contenido (o codificacion "
            "fragmentada)");
        return 0;
    }

    LOGF("medida respuesta http = {}", content_length);
    return content_length;
}

int VitaHttp::get_status()
{
    int res;
    int status;
    if ((res = sceHttpGetStatusCode(_http->req, &status)) < 0)
        throw HttpError(fmt::format(
                "Fallo sceHttpGetStatusCode: {:#08x}",
                static_cast<uint32_t>(res)));

    return status;
}

void VitaHttp::check_status()
{
    if (_status_checked)
        return;
    _status_checked = true;

    const auto status = get_status();

    LOGF("codigo estado http = {}", status);

    if (status != 200 && status != 206)
        throw HttpError(fmt::format("mal estado http: {}", status));
}

VitaHttp::operator bool() const
{
    return _http;
}

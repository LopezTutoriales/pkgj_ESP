#include "dialog.hpp"
#include "file.hpp"
#include "pkgi.hpp"
#include "vitahttp.hpp"

#include <boost/scope_exit.hpp>

#include <vector>

#define PKGJ_UPDATE_URL "https://raw.githubusercontent.com/blastrock/pkgj/last"
#define PKGJ_UPDATE_URL_VERSION PKGJ_UPDATE_URL "/version"

namespace
{
std::string version;

void start_download()
{
    try
    {
        LOGF("empezando descarga de version {}", version);

        const auto filename = fmt::format(
                "{}/pkgj-v{}.vpk", pkgi_get_config_folder(), version);
        const auto url =
                fmt::format("{}/pkgj-v{}.vpk", PKGJ_UPDATE_URL, version);

        pkgi_dialog_message("Descargando actualizacion", 0);

        try
        {
            const auto file = pkgi_create(filename.c_str());
            BOOST_SCOPE_EXIT_ALL(&)
            {
                pkgi_close(file);
            };

            VitaHttp http;
            http.start(url, 0);
            std::vector<uint8_t> data(64 * 1024);
            while (true)
            {
                const auto read = http.read(data.data(), data.size());
                if (read == 0)
                    break;
                pkgi_write(file, data.data(), read);
            }

            LOGF("descarga actualizacion completada");
        }
        catch (...)
        {
            LOGF("descarga fallida, elimina el archivo parcial");
            pkgi_rm(filename.c_str());
            throw;
        }

        pkgi_dialog_message(
                fmt::format(
                        "Actualizacion descargada en {}, instalala a "
                        "traves de VitaShell.",
                        filename)
                        .c_str());
    }
    catch (const std::exception& e)
    {
        pkgi_dialog_error(fmt::format("Descarga fallida: {}", e.what()).c_str());
    }
}

void update_thread()
{
    try
    {
        if (!pkgi_is_module_present("NoNpDrm"))
            pkgi_dialog_error(
                    "NoNpDrm no instalado. Los juegos no se podran instalar/jugar.");

        while (pkgi_dialog_is_open())
        {
            pkgi_sleep(20);
        }

        LOGF("comprobando ultima version en {}", PKGJ_UPDATE_URL_VERSION);

        VitaHttp http;
        http.start(PKGJ_UPDATE_URL_VERSION, 0);
        std::vector<uint8_t> last_versionb(10);
        last_versionb.resize(
                http.read(last_versionb.data(), last_versionb.size()));
        std::string last_version(last_versionb.begin(), last_versionb.end());

        LOGF("ultima version es {}", last_version);

        if (last_version != PKGI_VERSION)
        {
            LOG("nueva version disponible");

            version = last_version;

            pkgi_dialog_question(
                    fmt::format(
                            "Nueva version de PKGj {} disponible!\nQuieres "
                            "descargarla?",
                            last_version)
                            .c_str(),
                    {{"Si",
                      [] {
                          pkgi_start_thread(
                                  "pkgj_update_download", &start_download);
                      }},
                     {"No", [] {}}});
        }
    }
    catch (const std::exception& e)
    {
        LOGF("error en hilo de actualizacion: {}", e.what());
    }
}
}

void start_update_thread()
{
    pkgi_start_thread("pkgj_update", &update_thread);
}

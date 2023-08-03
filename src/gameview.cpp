#include "gameview.hpp"

#include <fmt/format.h>

#include "dialog.hpp"
#include "file.hpp"
#include "imgui.hpp"
extern "C"
{
#include "style.h"
}

namespace
{
constexpr unsigned GameViewWidth = VITA_WIDTH * 0.8;
constexpr unsigned GameViewHeight = VITA_HEIGHT * 0.8;
}

GameView::GameView(
        const Config* config,
        Downloader* downloader,
        DbItem* item,
        std::optional<CompPackDatabase::Item> base_comppack,
        std::optional<CompPackDatabase::Item> patch_comppack)
    : _config(config)
    , _downloader(downloader)
    , _item(item)
    , _base_comppack(base_comppack)
    , _patch_comppack(patch_comppack)
    , _patch_info_fetcher(item->titleid)
    , _image_fetcher(item)
{
    refresh();
}

void GameView::render()
{
    ImGui::SetNextWindowPos(
            ImVec2((VITA_WIDTH - GameViewWidth) / 2,
                   (VITA_HEIGHT - GameViewHeight) / 2));
    ImGui::SetNextWindowSize(ImVec2(GameViewWidth, GameViewHeight), 0);

    ImGui::Begin(
            fmt::format("{} ({})###gameview", _item->name, _item->titleid)
                    .c_str(),
            nullptr,
            ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove |
                    ImGuiWindowFlags_NoScrollbar |
                    ImGuiWindowFlags_NoScrollWithMouse |
                    ImGuiWindowFlags_NoCollapse |
                    ImGuiWindowFlags_NoSavedSettings);

    ImGui::PushTextWrapPos(
            _image_fetcher.get_texture() == nullptr ? 0.f
                                                    : GameViewWidth - 300.f);
    ImGui::Text(fmt::format("Version de firmware: {}", pkgi_get_system_version())
                        .c_str());
    ImGui::Text(
            fmt::format(
                    "Version de firmware requerida: {}", get_min_system_version())
                    .c_str());

    ImGui::Text(" ");

    ImGui::Text(fmt::format(
                        "Version del juego instalado: {}",
                        _game_version.empty() ? "no instalado" : _game_version)
                        .c_str());
    if (_comppack_versions.present && _comppack_versions.base.empty() &&
        _comppack_versions.patch.empty())
    {
        ImGui::Text("Pack de compatibilidad instalado: desconocido");
    }
    else
    {
        ImGui::Text(fmt::format(
                            "Pack de compatibilidad base instalado: {}",
                            _comppack_versions.base.empty() ? "no" : "si")
                            .c_str());
        ImGui::Text(fmt::format(
                            "Version pack de compatibilidad de parches instalado: {}",
                            _comppack_versions.patch.empty()
                                    ? "ninguno"
                                    : _comppack_versions.patch)
                            .c_str());
    }

    ImGui::Text(" ");

    printDiagnostic();

    ImGui::Text(" ");

    ImGui::PopTextWrapPos();

    if (_patch_info_fetcher.get_status() == PatchInfoFetcher::Status::Found)
    {
        if (ImGui::Button("Instalar juego y parche###installgame"))
            start_download_package();
    }
    else
    {
        if (ImGui::Button("Instalar juego###installgame"))
            start_download_package();
    }
    ImGui::SetItemDefaultFocus();

    if (_base_comppack)
    {
        if (!_downloader->is_in_queue(CompPackBase, _item->titleid))
        {
            if (ImGui::Button("Instalar pack de compatibilidad "
                              "base###installbasecomppack"))
                start_download_comppack(false);
        }
        else
        {
            if (ImGui::Button("Cancelar instalacion de pack de "
                              "compatibilidad base###installbasecomppack"))
                cancel_download_comppacks(false);
        }
    }
    if (_patch_comppack)
    {
        if (!_downloader->is_in_queue(CompPackPatch, _item->titleid))
        {
            if (ImGui::Button(fmt::format(
                                      "Instalar pack de compatibilidad "
                                      "{}###installpatchcommppack",
                                      _patch_comppack->app_version)
                                      .c_str()))
                start_download_comppack(true);
        }
        else
        {
            if (ImGui::Button("Cancelar instalacion de pack de parches "
                              "de compatibilidad###installpatchcommppack"))
                cancel_download_comppacks(true);
        }
    }

    auto tex = _image_fetcher.get_texture();
    // Display game image
    if (tex != nullptr)
    {
        int tex_w = vita2d_texture_get_width(tex);
        int tex_h = vita2d_texture_get_height(tex);
        float tex_x = ImGui::GetWindowContentRegionMax().x - tex_w;
        float tex_y = ImGui::GetWindowContentRegionMin().y;
        ImGui::SetCursorPos(ImVec2(tex_x, tex_y));
        ImGui::Image(tex, ImVec2(tex_w, tex_h));
    }

    ImGui::End();
}

static const auto Red = ImVec4(1.0f, 0.2f, 0.2f, 1.0f);
static const auto Yellow = ImVec4(1.0f, 1.0f, 0.0f, 1.0f);
static const auto Green = ImVec4(0.0f, 1.0f, 0.0f, 1.0f);

void GameView::printDiagnostic()
{
    bool ok = true;
    auto const printError = [&](auto const& str)
    {
        ok = false;
        ImGui::TextColored(Red, str);
    };

    auto const systemVersion = pkgi_get_system_version();
    auto const minSystemVersion = get_min_system_version();

    ImGui::Text("Diagnostico:");

    if (systemVersion < minSystemVersion)
    {
        if (!_comppack_versions.present)
        {
            if (_refood_present)
                ImGui::Text("- Este juego funcionara gracias a reF00D");
            else if (_0syscall6_present)
                ImGui::Text("- Este juego funcionara gracias a 0syscall6");
            else
                printError(
                        "- Tu firmware es muy bajo para jugar este juego, "
                        "debes instalar reF00D u 0syscall6");
        }
    }
    else
    {
        ImGui::Text("- Tu firmware es el mas reciente");
    }

    if (_comppack_versions.present && _comppack_versions.base.empty() &&
        _comppack_versions.patch.empty())
    {
        ImGui::TextColored(
                Yellow,
                "- Un pack de compatibilidad esta instalado pero no por "
                "PKGj, por favor, asegurese de que coincide con la version "
                "instalada o reinstalalo con PKGj.");
        ok = false;
    }

    if (_comppack_versions.base.empty() && !_comppack_versions.patch.empty())
        printError(
                "- Has instalado un pack de compatibilidad actualizado sin "
                "instalar el pack base, instala el pack base primero y "
                "reinstala la actualizacion del pack de compatibilidad.");

    std::string comppack_version;
    if (!_comppack_versions.patch.empty())
        comppack_version = _comppack_versions.patch;
    else if (!_comppack_versions.base.empty())
        comppack_version = _comppack_versions.base;

    if (_item->presence == PresenceInstalled && !comppack_version.empty() &&
        comppack_version < _game_version)
        printError(
                "- La version del juego no coincide con el pack de "
                "compatibilidad instalado. Si has actualizado el juego, "
                "instala tambien el pack de compatibilidad actualizado.");

    if (_item->presence == PresenceInstalled &&
        comppack_version > _game_version)
        printError(
                "- La version del juego no coincide con el pack de "
                "compatibilidad instalado. Downgradea a el pack base de "
                "compatibilidad o actualiza el juego desde el Live Area.");

    if (_item->presence != PresenceInstalled)
    {
        ImGui::Text("- Juego no instalado");
        ok = false;
    }

    if (ok)
        ImGui::TextColored(Green, "Todo verde");
}

std::string GameView::get_min_system_version()
{
    auto const patchInfo = _patch_info_fetcher.get_patch_info();
    if (patchInfo)
        return patchInfo->fw_version;
    else
        return _item->fw_version;
}

void GameView::refresh()
{
    LOGF("refrescando gameview");
    _refood_present = pkgi_is_module_present("ref00d");
    _0syscall6_present = pkgi_is_module_present("0syscall6");
    _game_version = pkgi_get_game_version(_item->titleid);
    _comppack_versions = pkgi_get_comppack_versions(_item->titleid);
}


void GameView::do_download() {
    pkgi_start_download(*_downloader, *_item);
    _item->presence = PresenceUnknown;
}

void GameView::start_download_package()
{
    if (_item->presence == PresenceInstalled)
    {
        LOGF("[{}] {} - ya instalado", _item->titleid, _item->name);
        pkgi_dialog_question(
        fmt::format(
                "{} ya esta instalado. "
                "Quieres volver a descargarlo?",
                _item->name)
                .c_str(),
        {{"Volver a descargar.", [this] { this->do_download(); }},
         {"No volver a descargar.", [] {} }});
        return;
    }
    this->do_download();
}

void GameView::cancel_download_package()
{
    _downloader->remove_from_queue(Game, _item->content);
    _item->presence = PresenceUnknown;
}

void GameView::start_download_comppack(bool patch)
{
    const auto& entry = patch ? _patch_comppack : _base_comppack;

    _downloader->add(DownloadItem{
            patch ? CompPackPatch : CompPackBase,
            _item->name,
            _item->titleid,
            _config->comppack_url + entry->path,
            std::vector<uint8_t>{},
            std::vector<uint8_t>{},
            false,
            "ux0:",
            entry->app_version});
}

void GameView::cancel_download_comppacks(bool patch)
{
    _downloader->remove_from_queue(
            patch ? CompPackPatch : CompPackBase, _item->titleid);
}

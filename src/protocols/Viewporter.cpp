#include "Viewporter.hpp"
#include "core/Compositor.hpp"
#include <algorithm>

CViewportResource::CViewportResource(SP<CWpViewport> resource_, SP<CWLSurfaceResource> surface_) : surface(surface_), resource(resource_) {
    if UNLIKELY (!good())
        return;

    resource->setDestroy([this](CWpViewport* r) { PROTO::viewport->destroyResource(this); });
    resource->setOnDestroy([this](CWpViewport* r) { PROTO::viewport->destroyResource(this); });

    resource->setSetDestination([this](CWpViewport* r, int32_t x, int32_t y) {
        if UNLIKELY (!surface) {
            r->error(WP_VIEWPORT_ERROR_NO_SURFACE, "Surface is gone");
            return;
        }

        surface->m_pending.updated.viewport = true;

        if (x == -1 && y == -1) {
            surface->m_pending.viewport.hasDestination = false;
            return;
        }

        if UNLIKELY (x <= 0 || y <= 0) {
            r->error(WP_VIEWPORT_ERROR_BAD_SIZE, "Size was <= 0");
            return;
        }

        surface->m_pending.viewport.hasDestination = true;
        surface->m_pending.viewport.destination    = {x, y};
    });

    resource->setSetSource([this](CWpViewport* r, wl_fixed_t fx, wl_fixed_t fy, wl_fixed_t fw, wl_fixed_t fh) {
        if UNLIKELY (!surface) {
            r->error(WP_VIEWPORT_ERROR_NO_SURFACE, "Surface is gone");
            return;
        }

        surface->m_pending.updated.viewport = true;

        double x = wl_fixed_to_double(fx), y = wl_fixed_to_double(fy), w = wl_fixed_to_double(fw), h = wl_fixed_to_double(fh);

        if (x == -1 && y == -1 && w == -1 && h == -1) {
            surface->m_pending.viewport.hasSource = false;
            return;
        }

        if UNLIKELY (x < 0 || y < 0) {
            r->error(WP_VIEWPORT_ERROR_BAD_SIZE, "Pos was < 0");
            return;
        }

        surface->m_pending.viewport.hasSource = true;
        surface->m_pending.viewport.source    = {x, y, w, h};
    });

    listeners.surfacePrecommit = surface->m_events.precommit.registerListener([this](std::any d) {
        if (!surface || !surface->m_pending.buffer)
            return;

        if (surface->m_pending.viewport.hasSource) {
            auto& src = surface->m_pending.viewport.source;

            if (src.w + src.x > surface->m_pending.bufferSize.x || src.h + src.y > surface->m_pending.bufferSize.y) {
                resource->error(WP_VIEWPORT_ERROR_BAD_VALUE, "Box doesn't fit");
                surface->m_pending.rejected = true;
                return;
            }
        }
    });
}

CViewportResource::~CViewportResource() {
    if (!surface)
        return;

    surface->m_pending.viewport.hasDestination = false;
    surface->m_pending.viewport.hasSource      = false;
}

bool CViewportResource::good() {
    return resource->resource();
}

CViewporterResource::CViewporterResource(SP<CWpViewporter> resource_) : resource(resource_) {
    if UNLIKELY (!good())
        return;

    resource->setDestroy([this](CWpViewporter* r) { PROTO::viewport->destroyResource(this); });
    resource->setOnDestroy([this](CWpViewporter* r) { PROTO::viewport->destroyResource(this); });

    resource->setGetViewport([](CWpViewporter* r, uint32_t id, wl_resource* surf) {
        const auto RESOURCE = PROTO::viewport->m_vViewports.emplace_back(
            makeShared<CViewportResource>(makeShared<CWpViewport>(r->client(), r->version(), id), CWLSurfaceResource::fromResource(surf)));

        if UNLIKELY (!RESOURCE->good()) {
            r->noMemory();
            PROTO::viewport->m_vViewports.pop_back();
            return;
        }
    });
}

bool CViewporterResource::good() {
    return resource->resource();
}

CViewporterProtocol::CViewporterProtocol(const wl_interface* iface, const int& ver, const std::string& name) : IWaylandProtocol(iface, ver, name) {
    ;
}

void CViewporterProtocol::bindManager(wl_client* client, void* data, uint32_t ver, uint32_t id) {
    const auto RESOURCE = m_vManagers.emplace_back(makeShared<CViewporterResource>(makeShared<CWpViewporter>(client, ver, id)));

    if UNLIKELY (!RESOURCE->good()) {
        wl_client_post_no_memory(client);
        m_vManagers.pop_back();
        return;
    }
}

void CViewporterProtocol::destroyResource(CViewporterResource* resource) {
    std::erase_if(m_vManagers, [&](const auto& other) { return other.get() == resource; });
}

void CViewporterProtocol::destroyResource(CViewportResource* resource) {
    std::erase_if(m_vViewports, [&](const auto& other) { return other.get() == resource; });
}

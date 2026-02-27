import { NavLink, Route, Routes, useLocation, useNavigate } from "react-router-dom";
import { Suspense, lazy, useState } from "react";
import { ToastProvider } from "./components/Toast";
import { ErrorBoundary } from "./components/ErrorBoundary";
import DashboardPage from "./pages/DashboardPage";
import ClientsPage from "./pages/ClientsPage";
import MissionsPage from "./pages/MissionsPage";

// P4: Lazy-load heavy / less-visited pages
const HdrPage = lazy(() => import("./pages/HdrPage"));
const GalleriesPage = lazy(() => import("./pages/GalleriesPage"));
const BillingPage = lazy(() => import("./pages/BillingPage"));
const PublicGalleryPage = lazy(() => import("./pages/PublicGalleryPage"));


const navItems = [
  { to: "/", label: "Dashboard", icon: <path d="M3 3h7v7H3V3zm11 0h7v7h-7V3zm0 11h7v7h-7v-7zM3 14h7v7H3v-7z" /> },
  { to: "/clients", label: "Clients", icon: <path d="M20 21v-2a4 4 0 0 0-4-4H8a4 4 0 0 0-4 4v2M12 7a4 4 0 1 0 0-8 4 4 0 0 0 0 8z" /> },
  { to: "/missions", label: "Missions", icon: <path d="M21 16V8a2 2 0 0 0-1-1.73l-7-4a2 2 0 0 0-2 0l-7 4A2 2 0 0 0 3 8v8a2 2 0 0 0 1 1.73l7 4a2 2 0 0 0 2 0l7-4A2 2 0 0 0 21 16zM3.3 7l8.7 5 8.7-5M12 22v-9" /> },
  { to: "/hdr", label: "HDR Studio", icon: <path d="M12 2v2m0 16v2M4.93 4.93l1.41 1.41m11.32 11.32l1.41 1.41M2 12h2m16 0h2M6.34 17.66l-1.41 1.41M19.07 4.93l-1.41 1.41" /> },
  { to: "/galleries", label: "Galeries", icon: <path d="M23 19a2 2 0 0 1-2 2H3a2 2 0 0 1-2-2V8a2 2 0 0 1 2-2h4l2-3h6l2 3h4a2 2 0 0 1 2 2zM12 13a4 4 0 1 0 0 8 4 4 0 0 0 0-8zm0 2a2 2 0 1 0 0 4 2 2 0 0 0 0-4z" /> },
  { to: "/billing", label: "Facturation", icon: <path d="M12 1v22M17 5H9.5a3.5 3.5 0 0 0 0 7h5a3.5 3.5 0 0 1 0 7H6" /> },
];

// Contextual top-bar actions per page
function TopBarActions({ pathname, fullscreen, setFullscreen, navigate }) {
  switch (pathname) {
    case "/missions":
      return (
        <>
          <button className="btn secondary" onClick={() => setFullscreen((f) => !f)}>
            {fullscreen ? "Quitter plein ecran" : "Plein ecran"}
          </button>
          <button className="btn secondary" onClick={() => navigate("/missions")}>Importer des RAW</button>
          <button className="btn" onClick={() => navigate("/missions#new")}>Nouvelle mission</button>
        </>
      );
    case "/hdr":
      return (
        <button className="btn secondary" onClick={() => setFullscreen((f) => !f)}>
          {fullscreen ? "Quitter plein ecran" : "Plein ecran"}
        </button>
      );
    case "/clients":
      return null; // Clients page manages its own buttons
    case "/billing":
      return null; // Billing page manages its own buttons
    case "/galleries":
      return null; // Galleries page manages its own buttons
    default:
      return (
        <>
          <button className="btn secondary" onClick={() => navigate("/missions")}>Importer des RAW</button>
          <button className="btn" onClick={() => navigate("/missions#new")}>Nouvelle mission</button>
        </>
      );
  }
}

export default function App() {
  const location = useLocation();
  const navigate = useNavigate();
  const [fullscreen, setFullscreen] = useState(false);
  const [sidebarCollapsed, setSidebarCollapsed] = useState(false);

  // Don't show app shell for public gallery page
  const isPublicPage = location.pathname.startsWith("/g/");
  if (isPublicPage) {
    return (
      <ToastProvider>
        <Suspense fallback={<div className="empty-state"><div className="empty-state-icon">⏳</div></div>}>
          <Routes>
            <Route path="/g/:token" element={<PublicGalleryPage />} />
          </Routes>
        </Suspense>
      </ToastProvider>
    );
  }

  return (
    <ToastProvider>
      <div className={`app-shell ${fullscreen ? "fullscreen" : ""} ${sidebarCollapsed ? "sidebar-collapsed" : ""}`}>
        <aside className="sidebar">
          <div className="sidebar-header">
            <h1 className="brand">
              <span className="brand-full">BlackLys</span>
              <span className="brand-compact">BL</span>
            </h1>
            <button
              className="icon-btn collapse-btn"
              type="button"
              onClick={() => setSidebarCollapsed((c) => !c)}
              aria-label={sidebarCollapsed ? "Deployer la barre latérale" : "Replier la barre latérale"}
            >
              <svg width="16" height="16" viewBox="0 0 24 24" fill="none" stroke="currentColor" strokeWidth="2" strokeLinecap="round" strokeLinejoin="round">
                <line x1="3" y1="12" x2="21" y2="12"></line>
                <line x1="3" y1="6" x2="21" y2="6"></line>
                <line x1="3" y1="18" x2="21" y2="18"></line>
              </svg>
            </button>
          </div>
          <nav>
            {navItems.map((item) => (
              <NavLink
                key={item.to}
                to={item.to}
                className={({ isActive }) =>
                  isActive ? "nav-link active" : "nav-link"
                }
                title={item.label}
              >
                <svg width="18" height="18" viewBox="0 0 24 24" fill="none" stroke="currentColor" strokeWidth="2" strokeLinecap="round" strokeLinejoin="round" className="nav-icon">
                  {item.icon}
                </svg>
                <span className="nav-text">{item.label}</span>
              </NavLink>
            ))}
          </nav>
        </aside>
        <main className={`content ${location.pathname === "/hdr" ? "hdr-mode" : ""}`}>
          <div className="top-bar">
            <div>
              <h2 style={{ margin: "10px 0 0" }}>{pageTitle(location.pathname)}</h2>
            </div>
            <div className="actions">
              <TopBarActions
                pathname={location.pathname}
                fullscreen={fullscreen}
                setFullscreen={setFullscreen}
                navigate={navigate}
              />
            </div>
          </div>
          <div className="page-enter">
            <ErrorBoundary>
              <Suspense fallback={<div className="empty-state"><div className="empty-state-icon">⏳</div></div>}>
                <Routes>
                  <Route path="/" element={<DashboardPage />} />
                  <Route path="/clients" element={<ClientsPage />} />
                  <Route path="/missions" element={<MissionsPage />} />
                  <Route path="/hdr" element={<HdrPage fullscreen={fullscreen} toggleFullscreen={() => setFullscreen(f => !f)} />} />
                  <Route path="/galleries" element={<GalleriesPage />} />
                  <Route path="/billing" element={<BillingPage />} />
                  <Route path="*" element={
                    <div className="empty-state">
                      <div className="empty-state-icon">🔍</div>
                      <div className="empty-state-title">Page introuvable</div>
                      <div className="empty-state-desc">Cette page n'existe pas</div>
                    </div>
                  } />
                </Routes>
              </Suspense>
            </ErrorBoundary>
          </div>
        </main>
      </div>
    </ToastProvider>
  );
}

function pageTitle(pathname) {
  switch (pathname) {
    case "/clients":
      return "Clients";
    case "/missions":
      return "Missions & shoots";
    case "/hdr":
      return "Studio HDR";
    case "/galleries":
      return "Galeries en ligne";
    case "/billing":
      return "Facturation & stats";
    default:
      return "Dashboard";
  }
}

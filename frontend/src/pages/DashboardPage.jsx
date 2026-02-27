import { useEffect, useState } from "react";
import { useNavigate } from "react-router-dom";
import { getDashboardStats, listMissions } from "../api";
import StatusBadge from "../components/StatusBadge";
import { SkeletonStats, SkeletonCards } from "../components/LoadingSpinner";

// P5: Module-level utility – not recreated per render
const fmtDate = (d) => d ? new Date(d).toLocaleDateString("fr-FR", { day: "2-digit", month: "short" }) : "";

export default function DashboardPage() {
  const navigate = useNavigate();
  const [stats, setStats] = useState(null);
  const [pipeline, setPipeline] = useState([]);
  const [loading, setLoading] = useState(true);
  const [error, setError] = useState("");

  useEffect(() => {
    async function load() {
      try {
        const [s, missions] = await Promise.all([
          getDashboardStats(),
          listMissions(),
        ]);
        setStats(s);
        setPipeline(missions.slice(0, 6));
      } catch (err) {
        setError(err.message);
      } finally {
        setLoading(false);
      }
    }
    load();
  }, []);

  if (error) {
    return (
      <div className="empty-state">
        <div className="empty-state-icon">⚠️</div>
        <div className="empty-state-title">Erreur de chargement</div>
        <div className="empty-state-desc">{error}</div>
      </div>
    );
  }

  const pipelineStats = stats
    ? [
      { label: "Planifiées", value: stats.planned, hint: "En attente de shoot", icon: "📅" },
      { label: "En traitement", value: stats.processing, hint: "HDR en cours", icon: "⚡" },
      { label: "Livrées", value: stats.delivered, hint: "Prêtes pour facturation", icon: "✅" },
    ]
    : [];

  const kpiStats = stats
    ? [
      { label: "Galeries actives", value: stats.active_galleries, hint: "Accessibles aux clients", icon: "🖼️" },
      { label: "CA (30j)", value: `${Number(stats.revenue_30d).toLocaleString("fr-FR")} €`, hint: `${stats.pending_count} facture(s) en attente`, icon: "💰" },
      { label: "Clients", value: stats.total_clients, hint: "Total enregistrés", icon: "👥" },
    ]
    : [];



  return (
    <div>
      {loading ? (
        <>
          <SkeletonStats count={6} />
          <SkeletonCards count={4} />
        </>
      ) : (
        <>
          {/* Primary pipeline counters */}
          <div className="stat-grid compact">
            {pipelineStats.map((s) => (
              <div key={s.label} className="stat-card">
                <div className="stat-label">{s.label}</div>
                <div className="stat-value">{s.value}</div>
                <div className="stat-hint">{s.icon} {s.hint}</div>
              </div>
            ))}
          </div>

          {/* Secondary KPIs */}
          <div className="stat-grid compact stat-grid-secondary">
            {kpiStats.map((s) => (
              <div key={s.label} className="stat-card">
                <div className="stat-label">{s.label}</div>
                <div className="stat-value">{s.value}</div>
                <div className="stat-hint">{s.icon} {s.hint}</div>
              </div>
            ))}
          </div>

          <div className="pipeline-section">
            <div className="pipeline-header">
              <h3>Pipeline récent</h3>
              <button className="btn secondary" onClick={() => navigate("/missions")}>
                Voir tout →
              </button>
            </div>
            {pipeline.length === 0 ? (
              <div className="empty-state">
                <div className="empty-state-icon">📷</div>
                <div className="empty-state-title">Aucune mission</div>
                <div className="empty-state-desc">Créez votre première mission pour commencer</div>
              </div>
            ) : (
              <div className="mission-grid">
                {pipeline.map((m) => (
                  <div key={m.id} className="mission-card">
                    <div className="mission-header">
                      <div>
                        <div className="mission-title">{m.title}</div>
                        <div className="mission-meta">
                          <span>{m.address}</span>
                          {(m.client?.name || m.client_name) && <span>• {m.client?.name || m.client_name}</span>}
                          {m.shoot_date && <span>• {fmtDate(m.shoot_date)}</span>}
                        </div>
                      </div>
                      <StatusBadge status={m.status} />
                    </div>
                    <div className="mission-actions">
                      <span className="stat-hint sm">{m.pack_name} – {m.price} €</span>
                      <button className="btn secondary" onClick={() => navigate("/missions")}>
                        Détails
                      </button>
                    </div>
                  </div>
                ))}
              </div>
            )}
          </div>
        </>
      )}
    </div>
  );
}

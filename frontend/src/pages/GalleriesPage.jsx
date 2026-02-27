import { useEffect, useState } from "react";
import { listGalleries, deleteGallery } from "../api";
import StatusBadge from "../components/StatusBadge";
import { SkeletonCards } from "../components/LoadingSpinner";
import { useToast } from "../components/Toast";
import PageHeader from "../components/PageHeader";

export default function GalleriesPage() {
  const toast = useToast();
  const [galleries, setGalleries] = useState([]);
  const [loading, setLoading] = useState(true);
  const [error, setError] = useState("");
  const [copiedId, setCopiedId] = useState(null);

  async function load() {
    try {
      const data = await listGalleries();
      setGalleries(data);
    } catch (err) {
      setError(err.message);
    } finally {
      setLoading(false);
    }
  }

  useEffect(() => { load(); }, []);

  async function copyLink(id, url) {
    try {
      await navigator.clipboard.writeText(url);
      setCopiedId(id);
      toast.success("Lien copié !");
      setTimeout(() => setCopiedId(null), 2000);
    } catch {
      toast.error("Échec de la copie");
    }
  }

  async function handleDelete(galleryId, missionId) {
    if (!confirm("Supprimer cette galerie ?")) return;
    try {
      await deleteGallery(missionId);
      setGalleries((prev) => prev.filter((g) => g.id !== galleryId));
      toast.success("Galerie supprimée");
    } catch (err) {
      toast.error(err.message);
    }
  }

  if (error) {
    return (
      <div className="empty-state">
        <div className="empty-state-icon">⚠️</div>
        <div className="empty-state-title">Erreur de chargement</div>
        <div className="empty-state-desc">{error}</div>
      </div>
    );
  }

  return (
    <div>
      <PageHeader title="Galeries" subtitle="Partage tes photos avec tes clients" />
      {loading ? (
        <SkeletonCards count={4} />
      ) : galleries.length === 0 ? (
        <div className="empty-state">
          <div className="empty-state-icon">🖼️</div>
          <div className="empty-state-title">Aucune galerie</div>
          <div className="empty-state-desc">Créez une galerie depuis une mission pour partager vos photos</div>
        </div>
      ) : (
        <div className="mission-grid">
          {galleries.map((g) => (
            <div key={g.id} className="mission-card">
              {/* Cover placeholder */}
              <div className="gallery-cover-placeholder">📷</div>

              <div className="mission-header">
                <div>
                  <div className="mission-title">{g.title}</div>
                  <div className="mission-meta">
                    <span>{g.photo_count} photo(s)</span>
                    <span>{g.created_at ? new Date(g.created_at).toLocaleDateString("fr-FR") : ""}</span>
                  </div>
                </div>
                <StatusBadge status={g.status} />
              </div>

              <div className="mission-actions">
                <button
                  className="copy-link-btn"
                  onClick={() => copyLink(g.id, g.share_url)}
                >
                  {copiedId === g.id ? "✓ Copié" : "🔗 Copier le lien"}
                </button>
                <a
                  href={g.share_url}
                  target="_blank"
                  rel="noreferrer"
                  className="btn secondary"
                >
                  Ouvrir
                </a>
                <button
                  className="btn secondary danger"
                  onClick={() => handleDelete(g.id, g.mission_id)}
                >
                  Supprimer
                </button>
              </div>
            </div>
          ))}
        </div>
      )}
    </div>
  );
}

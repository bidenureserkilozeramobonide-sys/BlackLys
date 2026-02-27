import { useEffect, useState } from "react";
import { useParams } from "react-router-dom";
import { api, API_BASE } from "../api";

export default function PublicGalleryPage() {
  const { token } = useParams();
  const [data, setData] = useState(null);
  const [loading, setLoading] = useState(true);
  const [error, setError] = useState("");

  useEffect(() => {
    async function load() {
      try {
        const res = await api.get(`/galleries/public/${token}`);
        setData(res.data);
      } catch (err) {
        setError(err?.response?.data?.detail || err.message);
      } finally {
        setLoading(false);
      }
    }
    load();
  }, [token]);

  if (loading) {
    return (
      <div style={styles.page}>
        <div style={styles.container}>
          <div style={styles.header}>
            <div className="skeleton skeleton-text" style={{ width: 200, height: 28 }} />
            <div className="skeleton skeleton-text" style={{ width: 120, marginTop: 8 }} />
          </div>
          <div style={styles.grid}>
            {Array.from({ length: 6 }).map((_, i) => (
              <div key={i} className="skeleton" style={{ height: 200, borderRadius: 12 }} />
            ))}
          </div>
        </div>
      </div>
    );
  }

  if (error) {
    return (
      <div style={styles.page}>
        <div style={styles.errorCard}>
          <div style={{ fontSize: 48, marginBottom: 16 }}>🔒</div>
          <h2 style={{ margin: "0 0 8px", color: "var(--text-main)" }}>Galerie inaccessible</h2>
          <p style={{ color: "var(--text-dim)", margin: 0 }}>{error}</p>
        </div>
      </div>
    );
  }

  return (
    <div style={styles.page}>
      <div style={styles.container}>
        {/* Header */}
        <div style={styles.header}>
          <div>
            <h1 style={styles.title}>{data.title}</h1>
            <p style={styles.subtitle}>
              {data.files?.length || 0} photo{(data.files?.length || 0) !== 1 ? "s" : ""}
            </p>
          </div>
          {data.zip_url && (
            <a
              href={`${API_BASE}${data.zip_url}`}
              download
              style={styles.downloadBtn}
            >
              📦 Télécharger tout (.zip)
            </a>
          )}
        </div>

        {/* Photo Grid */}
        {data.files && data.files.length > 0 ? (
          <div style={styles.grid}>
            {data.files.map((f) => (
              <a
                key={f.url}
                href={`${API_BASE}${f.url}`}
                target="_blank"
                rel="noreferrer"
                style={styles.photoCard}
              >
                {f.thumb_url ? (
                  <img src={f.thumb_url} alt={f.name} style={styles.photoImg} />
                ) : (
                  <div style={styles.photoPlaceholder}>📷</div>
                )}
                <div style={styles.photoName}>{f.name}</div>
              </a>
            ))}
          </div>
        ) : (
          <div style={styles.empty}>
            <div style={{ fontSize: 48, marginBottom: 12 }}>🖼️</div>
            <p style={{ color: "var(--text-dim)" }}>Aucune photo dans cette galerie</p>
          </div>
        )}

        {/* Footer */}
        <div style={styles.footer}>
          <span style={{ color: "var(--text-muted)", fontSize: 12 }}>
            Galerie propulsée par <strong>BlackLys</strong>
          </span>
        </div>
      </div>
    </div>
  );
}

const styles = {
  page: {
    minHeight: "100vh",
    background: "#000",
    color: "#fff",
    fontFamily: "'Space Grotesk', -apple-system, sans-serif",
    display: "flex",
    justifyContent: "center",
    padding: "40px 20px",
  },
  container: {
    width: "100%",
    maxWidth: 1100,
  },
  header: {
    display: "flex",
    justifyContent: "space-between",
    alignItems: "flex-start",
    marginBottom: 40,
    paddingBottom: 24,
    borderBottom: "1px solid #262626",
  },
  title: {
    fontSize: 28,
    fontWeight: 700,
    margin: 0,
    letterSpacing: "-0.02em",
  },
  subtitle: {
    fontSize: 14,
    color: "#808080",
    margin: "6px 0 0",
  },
  downloadBtn: {
    display: "inline-flex",
    alignItems: "center",
    gap: 8,
    padding: "10px 20px",
    borderRadius: 8,
    background: "rgba(255,255,255,0.06)",
    border: "1px solid #404040",
    color: "#fff",
    fontSize: 14,
    fontWeight: 500,
    textDecoration: "none",
    transition: "background 0.2s",
    cursor: "pointer",
    flexShrink: 0,
  },
  grid: {
    display: "grid",
    gridTemplateColumns: "repeat(auto-fill, minmax(280px, 1fr))",
    gap: 16,
  },
  photoCard: {
    background: "#121212",
    border: "1px solid #262626",
    borderRadius: 12,
    overflow: "hidden",
    textDecoration: "none",
    color: "#fff",
    transition: "transform 0.2s, border-color 0.2s",
    display: "block",
  },
  photoImg: {
    width: "100%",
    height: 200,
    objectFit: "cover",
    display: "block",
  },
  photoPlaceholder: {
    width: "100%",
    height: 200,
    display: "flex",
    alignItems: "center",
    justifyContent: "center",
    fontSize: 40,
    background: "#1a1a1a",
  },
  photoName: {
    padding: "10px 14px",
    fontSize: 13,
    color: "#a3a3a3",
    borderTop: "1px solid #262626",
    whiteSpace: "nowrap",
    overflow: "hidden",
    textOverflow: "ellipsis",
  },
  empty: {
    textAlign: "center",
    padding: "60px 20px",
  },
  errorCard: {
    background: "#121212",
    border: "1px solid #262626",
    borderRadius: 16,
    padding: 40,
    textAlign: "center",
    maxWidth: 400,
    margin: "0 auto",
  },
  footer: {
    textAlign: "center",
    marginTop: 48,
    paddingTop: 24,
    borderTop: "1px solid #262626",
  },
};

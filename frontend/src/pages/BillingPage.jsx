import { useEffect, useMemo, useState } from "react";
import { listInvoices } from "../api";
import StatusBadge from "../components/StatusBadge";
import { SkeletonStats } from "../components/LoadingSpinner";
import PageHeader from "../components/PageHeader";

const FILTERS = ["all", "draft", "sent", "paid"];
const FILTER_LABELS = { all: "Toutes", draft: "Brouillon", sent: "Envoyées", paid: "Payées" };

export default function BillingPage() {
  const [invoices, setInvoices] = useState([]);
  const [loading, setLoading] = useState(true);
  const [error, setError] = useState("");
  const [filter, setFilter] = useState("all");

  useEffect(() => {
    async function load() {
      try {
        // B17: Removed unused getDashboardStats() call
        const inv = await listInvoices();
        setInvoices(inv);
      } catch (err) {
        setError(err.message);
      } finally {
        setLoading(false);
      }
    }
    load();
  }, []);

  // B18: Memoize derived data
  const filtered = useMemo(
    () => filter === "all" ? invoices : invoices.filter((i) => i.status === filter),
    [invoices, filter]
  );
  const totalRevenue = useMemo(
    () => invoices.filter((i) => i.status === "paid").reduce((acc, i) => acc + i.amount, 0),
    [invoices]
  );
  const pendingAmount = useMemo(
    () => invoices.filter((i) => i.status !== "paid").reduce((acc, i) => acc + i.amount, 0),
    [invoices]
  );
  const pendingCount = useMemo(
    () => invoices.filter((i) => i.status !== "paid").length,
    [invoices]
  );

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
      <PageHeader title="Facturation" subtitle="Suivi des factures et revenus" />
      {loading ? (
        <SkeletonStats count={3} />
      ) : (
        <div className="stat-grid compact">
          <div className="stat-card">
            <div className="stat-label">Chiffre d'affaires</div>
            <div className="stat-value">{totalRevenue.toLocaleString("fr-FR")} €</div>
            <div className="stat-hint">💰 Total facturé encaissé</div>
          </div>
          <div className="stat-card">
            <div className="stat-label">En attente</div>
            <div className="stat-value">{pendingAmount.toLocaleString("fr-FR")} €</div>
            <div className="stat-hint">📝 {pendingCount} facture(s)</div>
          </div>
          <div className="stat-card">
            <div className="stat-label">Total factures</div>
            <div className="stat-value">{invoices.length}</div>
            <div className="stat-hint">📊 Depuis le début</div>
          </div>
        </div>
      )}

      {/* Filter tabs */}
      <div className="filter-tabs">
        {FILTERS.map((f) => (
          <button
            key={f}
            className={`btn ${filter === f ? "" : "secondary"}`}
            onClick={() => setFilter(f)}
          >
            {FILTER_LABELS[f]}
          </button>
        ))}
      </div>

      {/* Invoice table */}
      {!loading && filtered.length === 0 ? (
        <div className="empty-state">
          <div className="empty-state-icon">📄</div>
          <div className="empty-state-title">Aucune facture</div>
          <div className="empty-state-desc">
            {filter !== "all"
              ? "Aucune facture avec ce statut"
              : "Créez une facture depuis une mission livrée"}
          </div>
        </div>
      ) : (
        <div className="table-wrapper">
          <table className="invoice-table">
            <thead>
              <tr>
                <th>Mission</th>
                <th>Client</th>
                <th>Date</th>
                <th>Montant</th>
                <th>Statut</th>
              </tr>
            </thead>
            <tbody>
              {filtered.map((inv) => (
                <tr key={inv.id}>
                  <td className="font-medium">{inv.mission_title || inv.pack_name}</td>
                  <td>{inv.client_name || "—"}</td>
                  <td>{inv.issued_at ? new Date(inv.issued_at).toLocaleDateString("fr-FR") : "—"}</td>
                  <td className="invoice-amount">{inv.amount.toLocaleString("fr-FR")} {inv.currency}</td>
                  <td><StatusBadge status={inv.status} /></td>
                </tr>
              ))}
            </tbody>
          </table>
        </div>
      )}
    </div>
  );
}

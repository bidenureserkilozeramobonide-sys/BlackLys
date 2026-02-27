import { useEffect, useMemo, useState } from "react";
import { listClients, createClient, updateClient, deleteClient } from "../api";
import { useToast } from "../components/Toast";
import { SkeletonCards } from "../components/LoadingSpinner";
import PageHeader from "../components/PageHeader";

export default function ClientsPage() {
  const toast = useToast();
  const [clients, setClients] = useState([]);
  const [loading, setLoading] = useState(true);
  const [showForm, setShowForm] = useState(false);
  const [editId, setEditId] = useState(null);
  const [search, setSearch] = useState("");
  const [saving, setSaving] = useState(false);
  const [form, setForm] = useState({ name: "", company: "", email: "", phone: "" });

  async function load() {
    try {
      const data = await listClients();
      setClients(data);
    } catch (err) {
      toast.error(err.message);
    } finally {
      setLoading(false);
    }
  }

  useEffect(() => { load(); }, []);

  function resetForm() {
    setForm({ name: "", company: "", email: "", phone: "" });
    setEditId(null);
    setShowForm(false);
  }

  function startEdit(client) {
    setForm({
      name: client.name,
      company: client.company || "",
      email: client.email || "",
      phone: client.phone || "",
    });
    setEditId(client.id);
    setShowForm(true);
  }

  async function handleSubmit(e) {
    e.preventDefault();
    if (!form.name.trim()) return;
    setSaving(true);
    try {
      if (editId) {
        const updated = await updateClient(editId, form);
        setClients((prev) => prev.map((c) => (c.id === editId ? updated : c)));
        toast.success("Client mis à jour");
      } else {
        const newClient = await createClient(form);
        setClients((prev) => [...prev, newClient]);
        toast.success("Client créé");
      }
      resetForm();
    } catch (err) {
      toast.error(err.message);
    } finally {
      setSaving(false);
    }
  }

  async function handleDelete(id) {
    if (!confirm("Supprimer ce client ? Ses missions seront aussi supprimées.")) return;
    try {
      await deleteClient(id);
      setClients((prev) => prev.filter((c) => c.id !== id));
      toast.success("Client supprimé");
    } catch (err) {
      toast.error(err.message);
    }
  }

  // P2: Memoize filtered list
  const filtered = useMemo(() => clients.filter((c) => {
    const q = search.toLowerCase();
    return (
      c.name.toLowerCase().includes(q) ||
      (c.company || "").toLowerCase().includes(q) ||
      (c.email || "").toLowerCase().includes(q)
    );
  }), [clients, search]);

  return (
    <div>
      <PageHeader
        title={`${clients.length} client${clients.length !== 1 ? "s" : ""}`}
        subtitle="Gestion du carnet de contacts"
      >
        <input
          type="text"
          className="search-input"
          placeholder="Rechercher..."
          value={search}
          onChange={(e) => setSearch(e.target.value)}
        />
        <button className="btn" onClick={() => { resetForm(); setShowForm(true); }}>
          + Nouveau client
        </button>
      </PageHeader>

      {/* Create/Edit Form */}
      {showForm && (
        <div className="mission-card client-form-card">
          <h4 className="client-form-title">
            {editId ? "Modifier le client" : "Nouveau client"}
          </h4>
          <form onSubmit={handleSubmit} className="client-form-grid">
            <input
              className="form-input"
              placeholder="Nom *"
              required
              value={form.name}
              onChange={(e) => setForm({ ...form, name: e.target.value })}
            />
            <input
              className="form-input"
              placeholder="Société"
              value={form.company}
              onChange={(e) => setForm({ ...form, company: e.target.value })}
            />
            <input
              className="form-input"
              type="email"
              placeholder="Email"
              value={form.email}
              onChange={(e) => setForm({ ...form, email: e.target.value })}
            />
            <input
              className="form-input"
              placeholder="Téléphone"
              value={form.phone}
              onChange={(e) => setForm({ ...form, phone: e.target.value })}
            />
            <div className="client-form-actions">
              <button type="button" className="btn secondary" onClick={resetForm}>
                Annuler
              </button>
              <button type="submit" className="btn" disabled={saving}>
                {saving ? "..." : editId ? "Mettre à jour" : "Créer"}
              </button>
            </div>
          </form>
        </div>
      )}

      {/* Client cards */}
      {loading ? (
        <SkeletonCards count={6} />
      ) : filtered.length === 0 ? (
        <div className="empty-state">
          <div className="empty-state-icon">👥</div>
          <div className="empty-state-title">
            {search ? "Aucun résultat" : "Aucun client"}
          </div>
          <div className="empty-state-desc">
            {search ? "Essayez avec d'autres termes" : "Ajoutez votre premier client pour commencer"}
          </div>
        </div>
      ) : (
        <div className="mission-grid">
          {filtered.map((client) => (
            <div key={client.id} className="mission-card">
              <div className="mission-header">
                <div>
                  <div className="mission-title">{client.name}</div>
                  <div className="mission-meta">
                    <span>{client.company || "Indépendant"}</span>
                  </div>
                </div>
                <div className="client-avatar">{client.name.charAt(0).toUpperCase()}</div>
              </div>
              <div className="mission-meta client-contact-meta">
                {client.email && <span>✉ {client.email}</span>}
                {client.phone && <span>☎ {client.phone}</span>}
              </div>
              <div className="mission-actions">
                <button className="btn secondary" onClick={() => startEdit(client)}>
                  Modifier
                </button>
                <button
                  className="btn secondary danger"
                  onClick={() => handleDelete(client.id)}
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

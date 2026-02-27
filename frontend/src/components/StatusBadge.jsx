const STATUS_LABELS = {
    // Mission statuses
    planned: "Planifiée",
    processing: "En cours",
    delivered: "Livrée",
    paid: "Payée",
    // Invoice statuses
    draft: "Brouillon",
    sent: "Envoyée",
    overdue: "En retard",
    // Gallery statuses
    active: "Active",
    expired: "Expirée",
};

export default function StatusBadge({ status, className = "" }) {
    const label = STATUS_LABELS[status] || status;
    // Map gallery "active" to "delivered" CSS class for green badge
    const cssClass = status === "active" ? "delivered" : status;
    return (
        <span className={`status-badge-clean ${cssClass} ${className}`}>
            {label}
        </span>
    );
}

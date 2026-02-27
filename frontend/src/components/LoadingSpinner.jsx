export function SkeletonCards({ count = 3 }) {
    return (
        <div className="mission-grid">
            {Array.from({ length: count }).map((_, i) => (
                <div key={i} className="skeleton skeleton-card" />
            ))}
        </div>
    );
}

export function SkeletonStats({ count = 4 }) {
    return (
        <div className="stat-grid">
            {Array.from({ length: count }).map((_, i) => (
                <div key={i} className="stat-card" style={{ minHeight: 110 }}>
                    <div className="skeleton skeleton-text" style={{ width: '60%' }} />
                    <div className="skeleton skeleton-text lg" />
                    <div className="skeleton skeleton-text" style={{ width: '40%', marginTop: 8 }} />
                </div>
            ))}
        </div>
    );
}

export function LoadingPage() {
    return (
        <div className="page-enter" style={{ padding: 20 }}>
            <div style={{ marginBottom: 30 }}>
                <div className="skeleton skeleton-text" style={{ width: 180, height: 24 }} />
                <div className="skeleton skeleton-text" style={{ width: 120, marginTop: 8 }} />
            </div>
            <SkeletonStats />
            <SkeletonCards />
        </div>
    );
}

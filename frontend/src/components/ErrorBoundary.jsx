import React from "react";

export class ErrorBoundary extends React.Component {
    constructor(props) {
        super(props);
        this.state = { hasError: false, error: null };
    }

    static getDerivedStateFromError(error) {
        return { hasError: true, error };
    }

    componentDidCatch(error, errorInfo) {
        console.error("ErrorBoundary caught:", error, errorInfo);
    }

    render() {
        if (this.state.hasError) {
            return (
                <div className="empty-state" style={{ minHeight: "60vh" }}>
                    <div className="empty-state-icon">💥</div>
                    <div className="empty-state-title">Erreur inattendue</div>
                    <div className="empty-state-desc">
                        {this.state.error?.message || "Quelque chose s'est mal passé"}
                    </div>
                    <button
                        className="btn"
                        style={{ marginTop: 20 }}
                        onClick={() => {
                            this.setState({ hasError: false, error: null });
                            window.location.reload();
                        }}
                    >
                        Recharger la page
                    </button>
                </div>
            );
        }
        return this.props.children;
    }
}

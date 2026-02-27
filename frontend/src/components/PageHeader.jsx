/**
 * Reusable page header with title, subtitle, and action buttons.
 * Eliminates inline style duplication across pages.
 */
export default function PageHeader({ title, subtitle, children }) {
    return (
        <div className="page-header">
            <div>
                <h3 className="page-header-title">{title}</h3>
                {subtitle && <div className="page-header-subtitle">{subtitle}</div>}
            </div>
            {children && <div className="page-header-actions">{children}</div>}
        </div>
    );
}

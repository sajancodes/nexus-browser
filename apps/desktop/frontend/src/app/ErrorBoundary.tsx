import { Component, type ErrorInfo, type ReactNode } from 'react';

interface AppErrorBoundaryProps {
  children: ReactNode;
}

interface AppErrorBoundaryState {
  error: Error | null;
}

export class AppErrorBoundary extends Component<
  AppErrorBoundaryProps,
  AppErrorBoundaryState
> {
  state: AppErrorBoundaryState = { error: null };

  static getDerivedStateFromError(error: Error): AppErrorBoundaryState {
    return { error };
  }

  componentDidCatch(error: Error, errorInfo: ErrorInfo): void {
    console.error('[NEXUS] shell render failure', error, errorInfo);
  }

  render() {
    if (this.state.error) {
      return (
        <div className="boot-error" role="alert">
          <div className="boot-error-text">
            <strong>The NEXUS shell hit an unexpected error.</strong>
            <span>{this.state.error.message}</span>
          </div>
          <button
            type="button"
            className="retry-button"
            onClick={() => window.location.reload()}
          >
            Reload shell
          </button>
        </div>
      );
    }
    return this.props.children;
  }
}

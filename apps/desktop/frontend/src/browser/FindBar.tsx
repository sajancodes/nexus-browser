import { useEffect, useRef, useState, type FormEvent } from 'react';
import { ArrowLeftIcon, CloseIcon, ForwardIcon, SearchIcon } from '../components/icons';

interface FindBarProps {
  onFind: (query: string, forward?: boolean, findNext?: boolean) => void;
  onClose: () => void;
}

export function FindBar({ onFind, onClose }: FindBarProps) {
  const [query, setQuery] = useState('');
  const inputRef = useRef<HTMLInputElement>(null);

  useEffect(() => {
    inputRef.current?.focus();
    inputRef.current?.select();
  }, []);

  const handleSubmit = (event: FormEvent) => {
    event.preventDefault();
    if (query.trim()) {
      onFind(query, true, true);
    }
  };

  const handleNext = () => {
    if (query.trim()) {
      onFind(query, true, true);
    }
  };

  const handlePrevious = () => {
    if (query.trim()) {
      onFind(query, false, true);
    }
  };

  return (
    <div className="find-bar" role="search">
      <SearchIcon className="find-icon" />
      <form onSubmit={handleSubmit} className="find-form">
        <input
          ref={inputRef}
          type="text"
          className="find-input"
          placeholder="Find in page…"
          value={query}
          onChange={(e) => {
            setQuery(e.target.value);
            onFind(e.target.value, true, false);
          }}
          onKeyDown={(e) => {
            if (e.key === 'Escape') {
              onClose();
            }
          }}
        />
      </form>
      <div className="find-actions">
        <button
          type="button"
          className="icon-button"
          onClick={handlePrevious}
          title="Previous match (Shift+Enter)"
          aria-label="Previous match"
        >
          <ArrowLeftIcon />
        </button>
        <button
          type="button"
          className="icon-button"
          onClick={handleNext}
          title="Next match (Enter)"
          aria-label="Next match"
        >
          <ForwardIcon />
        </button>
        <button
          type="button"
          className="icon-button"
          onClick={onClose}
          title="Close find bar (Esc)"
          aria-label="Close find bar"
        >
          <CloseIcon />
        </button>
      </div>
    </div>
  );
}

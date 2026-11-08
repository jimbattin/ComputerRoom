import type { ComponentChildren } from "preact";
import { useEffect, useRef } from "preact/hooks";

/** `text`, plus the value as last loaded or saved when it differs (`original` set). */
export const withOriginal = (text: string, original: string | undefined): string =>
  original === undefined ? text : `${text}\nOriginal: ${original}`;

/** Pointer and keyboard focus handlers that report `key` to `onHover`, and null on leave. */
export const hoverHandlers = (key: string, onHover: (key: string | null) => void) => ({
  onMouseEnter: () => onHover(key),
  onMouseLeave: () => onHover(null),
  onFocusIn: () => onHover(key),
  onFocusOut: () => onHover(null),
});

export function Led({ text, wide, small, off }: { text: string; wide?: boolean; small?: boolean; off?: boolean }) {
  const cls = ["led", wide && "wide", small && "small", off && "off"].filter(Boolean).join(" ");
  return (
    <div class={cls} title={text}>
      {text}
    </div>
  );
}

export interface KeyProps {
  label: string;
  pressed?: boolean;
  disabled?: boolean;
  dark?: boolean;
  wide?: boolean;
  title?: string;
  /** Edit mark in the top-right corner: a dot for changed, a ring for added since load/save. */
  mark?: "edited" | "added";
  onClick: () => void;
}

export function Key({ label, pressed, disabled, dark, wide, title, mark, onClick }: KeyProps) {
  return (
    <button
      type="button"
      class={["key", dark && "dark", wide && "wide", mark].filter(Boolean).join(" ")}
      aria-pressed={pressed ? "true" : "false"}
      disabled={disabled}
      title={title}
      onClick={onClick}
    >
      <span class="key-led" />
      {mark && <span class="edit-mark" />}
      {label}
    </button>
  );
}

export interface NumberFieldProps {
  value: number;
  min: number;
  max: number;
  step: number;
  /** Text shown for `value`. */
  display: (v: number) => string;
  onCommit: (v: number) => void;
  label?: string;
  class?: string;
}

/**
 * A number input that commits on `change`. Input that is not a number or is outside min..max
 * reverts to the current value; a field with a whole-number step rounds.
 */
export function NumberField({ value, min, max, step, display, onCommit, label, class: cls }: NumberFieldProps) {
  return (
    <input
      type="number"
      class={cls}
      aria-label={label}
      min={min}
      max={max}
      step={step}
      value={display(value)}
      onChange={(e) => {
        const el = e.currentTarget;
        let n = el.value.trim() === "" ? NaN : Number(el.value);
        if (!Number.isFinite(n) || n < min || n > max) {
          el.value = display(value);
          return;
        }
        if (Number.isInteger(step)) n = Math.round(n);
        el.value = display(n);
        onCommit(n);
      }}
    />
  );
}

export interface ConfirmDialogProps {
  title: string;
  body: ComponentChildren;
  confirmLabel: string;
  onConfirm: () => void;
  onCancel: () => void;
}

/**
 * A modal confirmation, shown while mounted. showModal() makes the page behind it inert and moves
 * focus to the first button (Cancel); Escape cancels.
 */
export function ConfirmDialog({ title, body, confirmLabel, onConfirm, onCancel }: ConfirmDialogProps) {
  const ref = useRef<HTMLDialogElement>(null);
  useEffect(() => ref.current?.showModal(), []);
  return (
    <dialog
      ref={ref}
      onCancel={(e) => {
        // Escape: unmounting closes the dialog, so the browser need not.
        e.preventDefault();
        onCancel();
      }}
    >
      <article>
        <header>
          <strong>{title}</strong>
        </header>
        <p>{body}</p>
        <footer>
          <button type="button" class="secondary" onClick={onCancel}>
            Cancel
          </button>{" "}
          <button type="button" onClick={onConfirm}>
            {confirmLabel}
          </button>
        </footer>
      </article>
    </dialog>
  );
}

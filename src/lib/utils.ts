import { clsx, type ClassValue } from "clsx";
import { twMerge } from "tailwind-merge";

export function cn(...inputs: ClassValue[]) {
	return twMerge(clsx(inputs));
}

// eslint-disable-next-line @typescript-eslint/no-explicit-any
export type WithoutChild<T> = T extends { child?: any } ? Omit<T, "child"> : T;
// eslint-disable-next-line @typescript-eslint/no-explicit-any
export type WithoutChildren<T> = T extends { children?: any } ? Omit<T, "children"> : T;
export type WithoutChildrenOrChild<T> = WithoutChildren<WithoutChild<T>>;
export type WithElementRef<T, U extends HTMLElement = HTMLElement> = T & { ref?: U | null };

/**
 * Format a Date to 24-hour time string (HH:MM)
 */
export function formatTime24h(date: Date): string {
	return date.toLocaleTimeString('en-GB', { 
		hour: '2-digit', 
		minute: '2-digit', 
		hour12: false 
	});
}

/**
 * Calculate wheel circumference in meters from diameter in inches
 */
export function getWheelCircumference(diameterInches: string | number): number {
	const parsed = typeof diameterInches === 'string' 
		? Number.parseInt(diameterInches, 10) 
		: diameterInches;
	// Convert diameter in inches to circumference in meters
	// diameter (inches) × π × 0.0254 (inches to meters)
	return Number.isFinite(parsed) ? (parsed * Math.PI * 0.0254) : 0;
}

/**
 * Calculate distance in kilometers from rotations and wheel circumference
 */
export function calculateDistance(rotations: number, wheelCircumferenceMeters: number): number {
	return (rotations * wheelCircumferenceMeters) / 1000;
}

/**
 * Format distance in kilometers to fixed decimal places
 */
export function formatDistanceKm(km: number, decimals: number = 2): string {
	return km.toFixed(decimals);
}
# ADR 0004: Use provider-neutral connector capabilities

- Status: Accepted
- Date: 2026-08-02

## Context

Households use different media, storage, notification, and automation services. Coupling the core domain to one provider would create hidden dependencies and leak provider behavior into policy.

## Decision

Core Sprout behavior will depend on connector categories and declared capabilities. Provider adapters own credentials, configuration, health, caching, and unsupported features. Connectors are optional and removable.

## Consequences

Offline launcher behavior cannot depend on a connector. Provider-specific fields stay outside core models. No connector framework is implemented until a milestone supplies a real consumer.
